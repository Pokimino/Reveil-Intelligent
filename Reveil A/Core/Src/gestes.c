/* gestes.c (carte A) : tâche Gestes -- STOP et SNOOZE sans contact (suit le diagramme de la conception)
 *   TOF1 = capteur gauche (dG), TOF2 = capteur droit (dD), même code d'initialisation que le cours
 *   - une main qui PASSE au-dessus d'un capteur (présente entre T_MIN et T_MAX, validée quand elle
 *     repart) -> SNOOZE
 *   - deux mains maintenues pendant T_STOP -> STOP (prioritaire : si la 2e main arrive, on oublie
 *     le snooze)
 *   - les gestes ne sont envoyés au Réveil que si l'alarme sonne ou est en snooze
 *   Méthode du cours temps réel : interruption INTOF -> sémaphore -> la tâche lit la mesure.
 *   Bus I2C partagé (OLED, capteur d'air) : chaque accès aux ToF prend I2C1_MutexHandle.
 */
#include "gestes.h"
#include "partage.h"
#include "app_msg.h"
#include "main.h"
#include <string.h>
#include "../../Drivers/VL53L1_Nucleo/core/inc/vl53l1_api.h"

extern I2C_HandleTypeDef hi2c1;

/* Durées des gestes (plus tard : Config.gestes) */
#define T_MIN_MS       100    /* passage plus court : ignoré (parasite) */
#define T_MAX_MS      1500    /* passage plus long : ce n'est pas un snooze */
#define T_STOP_MS     3000    /* deux mains maintenues 3 s = STOP */
#define ABSENCE_MS     200    /* plus de mesure depuis 200 ms : la main est partie */

static VL53L1_Dev_t tof[2];
static osSemaphoreId_t SemToFHandle;
static volatile uint8_t tof_pret[2];

typedef enum { ATTENTE, UNE_MAIN, DEUX_MAINS } EtatGeste_t;

/* Pour le debug (Live Expressions) */
volatile uint16_t   distance_geste[2];      /* dernière distance mesurée (mm) */
volatile uint8_t    main_presente[2];
volatile EtatGeste_t etat_geste;
volatile Geste_t    dernier_geste;          /* dernier geste reconnu (même si l'alarme ne sonne pas) */
volatile uint32_t   nb_gestes;              /* nombre de gestes reconnus */


/* ================== Initialisation d'un ToF (code du cours, page 106) ================== */
static void InitialiserToF(VL53L1_DEV Dev, GPIO_TypeDef *port_shut, uint16_t pin_shut, uint8_t newI2C)
{
  VL53L1_DetectionConfig_t DetectionConfig;
  VL53L1_UserRoi_t roiConfig;

  HAL_GPIO_WritePin(port_shut, pin_shut, GPIO_PIN_SET);   /* sortie du reset */
  HAL_Delay(5);
  Dev->comms_speed_khz = 400;
  Dev->I2cHandle = &hi2c1;
  Dev->comms_type = 1;
  Dev->I2cDevAddr = 0x52;                /* adresse d'usine, commune à tous les ToF */
  VL53L1_SetDeviceAddress(Dev, newI2C);  /* nouvelle adresse, sinon conflit avec l'autre ToF */
  Dev->I2cDevAddr = newI2C;

  VL53L1_WaitDeviceBooted(Dev);
  VL53L1_DataInit(Dev);
  VL53L1_StaticInit(Dev);
  VL53L1_SetDistanceMode(Dev, VL53L1_DISTANCEMODE_LONG);
  VL53L1_SetMeasurementTimingBudgetMicroSeconds(Dev, 40000);
  VL53L1_SetInterMeasurementPeriodMilliSeconds(Dev, 50);
  memset(&DetectionConfig, 0, sizeof(DetectionConfig));
  DetectionConfig.DetectionMode = VL53L1_DETECTION_DISTANCE_ONLY;
  DetectionConfig.IntrNoTarget = 0;
  DetectionConfig.Distance.CrossMode = VL53L1_THRESHOLD_IN_WINDOW;
  DetectionConfig.Distance.Low = 20;      /* main détectée entre 2 cm ... */
  DetectionConfig.Distance.High = 300;    /* ... et 30 cm */
  VL53L1_SetThresholdConfig(Dev, &DetectionConfig);
  roiConfig.TopLeftX = 0;
  roiConfig.TopLeftY = 15;
  roiConfig.BotRightX = 15;
  roiConfig.BotRightY = 0;
  VL53L1_SetUserROI(Dev, &roiConfig);
  VL53L1_ClearInterruptAndStartMeasurement(Dev);
}


/* ================== Interruption des ToF (INTOF1 / INTOF2, front descendant) ================== */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == INTOF1_Pin)
  {
    tof_pret[0] = 1;
    osSemaphoreRelease(SemToFHandle);
  }
  else if (GPIO_Pin == INTOF2_Pin)
  {
    tof_pret[1] = 1;
    osSemaphoreRelease(SemToFHandle);
  }
}


/* Geste reconnu : on l'envoie au Réveil seulement si l'alarme sonne ou est en snooze */
static void geste_reconnu(Geste_t g)
{
  EtatReveil_t r;

  dernier_geste = g;
  nb_gestes++;

  EtatReveil_Lire(&r);
  if (r.etat != APP_ALARM_SILENCIEUSE)
  {
    osMessageQueuePut(Evt_GestesHandle, &g, 0, 0);
  }
}


/* ================== Tâche ================== */
static void Gestes_Tache(void *argument)
{
  uint32_t derniere_mesure[2] = { 0, 0 };
  uint32_t debut = 0;                      /* début du passage (UNE_MAIN) ou du maintien (DEUX_MAINS) */
  EtatGeste_t etat = ATTENTE;

  /* Les 2 ToF en reset, puis initialisation un par un (bus I2C réservé pendant ce temps) */
  osMutexAcquire(I2C1_MutexHandle, osWaitForever);
  HAL_GPIO_WritePin(SHUT1_GPIO_Port, SHUT1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SHUT2_GPIO_Port, SHUT2_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  InitialiserToF(&tof[0], SHUT1_GPIO_Port, SHUT1_Pin, 0x54);
  InitialiserToF(&tof[1], SHUT2_GPIO_Port, SHUT2_Pin, 0x56);
  osMutexRelease(I2C1_MutexHandle);

  for (;;)
  {
    /* Réveil par l'interruption d'un ToF, ou au plus tard toutes les 50 ms */
    osSemaphoreAcquire(SemToFHandle, 50);
    uint32_t maintenant = osKernelGetTickCount();

    /* 1) Lecture des mesures prêtes : une mesure = une main dans la fenêtre 2-30 cm */
    for (int i = 0; i < 2; i++)
    {
      if (tof_pret[i])
      {
        VL53L1_RangingMeasurementData_t mesure;

        tof_pret[i] = 0;
        osMutexAcquire(I2C1_MutexHandle, osWaitForever);
        VL53L1_GetRangingMeasurementData(&tof[i], &mesure);
        VL53L1_ClearInterruptAndStartMeasurement(&tof[i]);
        osMutexRelease(I2C1_MutexHandle);

        if (mesure.RangeStatus == 0)
        {
          distance_geste[i] = mesure.RangeMilliMeter;
          derniere_mesure[i] = maintenant;
        }
      }
      main_presente[i] = (derniere_mesure[i] != 0) && (maintenant - derniere_mesure[i] < ABSENCE_MS);
    }

    uint8_t nb_mains = main_presente[0] + main_presente[1];

    /* 2) Diagramme d'états de la conception */
    switch (etat)
    {
      case ATTENTE:
        if (nb_mains == 2)      { etat = DEUX_MAINS; debut = maintenant; }
        else if (nb_mains == 1) { etat = UNE_MAIN;   debut = maintenant; }
        break;

      case UNE_MAIN:
        if (nb_mains == 2)                         /* 2e main : le STOP est prioritaire */
        {
          etat = DEUX_MAINS;
          debut = maintenant;
        }
        else if (nb_mains == 0)                    /* main retirée : était-ce un passage ? */
        {
          uint32_t duree = maintenant - debut;
          if (duree >= T_MIN_MS && duree <= T_MAX_MS)
          {
            geste_reconnu(GESTE_SNOOZE);
          }
          etat = ATTENTE;
        }
        break;

      case DEUX_MAINS:
        if (nb_mains < 2)                          /* une main retirée avant T_STOP : abandon */
        {
          etat = ATTENTE;
        }
        else if (maintenant - debut >= T_STOP_MS)  /* maintien réussi */
        {
          geste_reconnu(GESTE_STOP);
          etat = ATTENTE;
        }
        break;
    }
    etat_geste = etat;
  }
}

void Gestes_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Gestes",
    .stack_size = 1024 * 4,      /* l'API des ToF utilise beaucoup de pile */
    .priority = osPriorityNormal
  };
  SemToFHandle = osSemaphoreNew(1, 0, NULL);
  osThreadNew(Gestes_Tache, NULL, &attr);
}
