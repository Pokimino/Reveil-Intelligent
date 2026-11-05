/* capteurs_sommeil.c : cartes B et C
 *   Mesure les 2 capteurs de distance (ToF) et envoie les distances à la carte A par le bus CAN.
 *     Carte B : TOF1 = d1 (tête),   TOF2 = d2 (tronc)   -> identifiant CAN 0x21
 *     Carte C : TOF1 = d3 (bassin), TOF2 = d4 (jambes)  -> identifiant CAN 0x22
 *   Même programme pour les 2 cartes : seule la constante CARTE change avant de flasher.
 *
 *   Méthode du cours temps réel : l'interruption du ToF (INTOF) libère un sémaphore,
 *   la tâche qui attendait ce sémaphore lit la mesure (pas d'I2C dans l'interruption).
 *   Initialisation des ToF : code du cours (ResetAndInitializeTOF1), fait pour les 2 capteurs.
 */
#include "capteurs_sommeil.h"
#include "main.h"
#include "cmsis_os.h"
#include <string.h>
#include "../../Drivers/VL53L1_Nucleo/core/inc/vl53l1_api.h"

extern I2C_HandleTypeDef hi2c1;
extern CAN_HandleTypeDef hcan1;

/* ====== À changer avant de flasher : 'B' ou 'C' ====== */
static const char CARTE = 'C';

/* Format des messages CAN : doit être le même sur la carte A */
#define ID_CAN_B           0x21
#define ID_CAN_C           0x22
#define INVALIDE           0xFFFF   /* distance envoyée quand la mesure n'est pas valable */
#define PERIODE_CAN_MS     100      /* un message toutes les 100 ms (comme le TP CAN) */
#define TIMEOUT_MESURE_MS  500      /* pas de nouvelle mesure depuis 0,5 s : distance invalide */

static VL53L1_Dev_t tof[2];               /* TOF1 et TOF2 */
static osSemaphoreId_t SemToFHandle;      /* libéré par l'interruption d'un ToF */
static volatile uint8_t tof_pret[2];      /* quel ToF a une mesure prête */

/* Pour le debug (Live Expressions) */
volatile uint16_t distance[2] = { INVALIDE, INVALIDE };   /* en mm */
volatile uint8_t  statut[2];                              /* 0 = mesure valable */


/* ================== Initialisation d'un ToF (code du cours, page 106) ================== */
static void InitialiserToF(VL53L1_DEV Dev, GPIO_TypeDef *port_shut, uint16_t pin_shut, uint8_t newI2C)
{
  VL53L1_DetectionConfig_t DetectionConfig;
  VL53L1_UserRoi_t roiConfig;

  /* On sort ce ToF du reset et on change son adresse I2C :
     d'usine, tous les ToF ont l'adresse 0x52, sinon ils seraient en conflit */
  HAL_GPIO_WritePin(port_shut, pin_shut, GPIO_PIN_SET);
  HAL_Delay(5);
  Dev->comms_speed_khz = 400;
  Dev->I2cHandle = &hi2c1;
  Dev->comms_type = 1;
  Dev->I2cDevAddr = 0x52;            /* adresse d'usine */
  VL53L1_SetDeviceAddress(Dev, newI2C);
  Dev->I2cDevAddr = newI2C;

  /* Initialisation et réglages (identiques au cours) */
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
  DetectionConfig.Distance.Low = 20;      /* mesure entre 2 cm ... */
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
/* Pas d'I2C ici : on note quel ToF est prêt et on réveille la tâche. */
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


/* ================== Envoi CAN : 4 octets = 2 distances en mm (poids fort en premier) ================== */
static void envoyer_can(void)
{
  CAN_TxHeaderTypeDef entete;
  uint8_t data[4];
  uint32_t mailbox;

  entete.StdId = (CARTE == 'B') ? ID_CAN_B : ID_CAN_C;
  entete.IDE = CAN_ID_STD;
  entete.RTR = CAN_RTR_DATA;
  entete.DLC = 4;
  entete.TransmitGlobalTime = DISABLE;

  data[0] = distance[0] >> 8;
  data[1] = distance[0] & 0xFF;
  data[2] = distance[1] >> 8;
  data[3] = distance[1] & 0xFF;

  if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) > 0)
  {
    HAL_CAN_AddTxMessage(&hcan1, &entete, data, &mailbox);
  }
}


/* ================== Tâche ================== */
static void CapteursSommeil_Tache(void *argument)
{
  uint32_t derniere_mesure[2];
  uint32_t dernier_envoi;

  /* Les 2 ToF en reset, puis on les initialise un par un (changement d'adresse) */
  HAL_GPIO_WritePin(SHUT1_GPIO_Port, SHUT1_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(SHUT2_GPIO_Port, SHUT2_Pin, GPIO_PIN_RESET);
  HAL_Delay(10);
  InitialiserToF(&tof[0], SHUT1_GPIO_Port, SHUT1_Pin, 0x54);
  InitialiserToF(&tof[1], SHUT2_GPIO_Port, SHUT2_Pin, 0x56);

  HAL_CAN_Start(&hcan1);

  derniere_mesure[0] = derniere_mesure[1] = dernier_envoi = osKernelGetTickCount();

  for (;;)
  {
    /* Réveil par l'interruption d'un ToF, ou au plus tard au bout de 100 ms */
    osSemaphoreAcquire(SemToFHandle, PERIODE_CAN_MS);
    uint32_t maintenant = osKernelGetTickCount();

    for (int i = 0; i < 2; i++)
    {
      if (tof_pret[i])
      {
        VL53L1_RangingMeasurementData_t mesure;

        tof_pret[i] = 0;
        VL53L1_GetRangingMeasurementData(&tof[i], &mesure);
        statut[i] = mesure.RangeStatus;
        if (mesure.RangeStatus == 0)                 /* mesure valable */
        {
          distance[i] = mesure.RangeMilliMeter;
          derniere_mesure[i] = maintenant;
        }
        VL53L1_ClearInterruptAndStartMeasurement(&tof[i]);
      }

      /* Plus rien dans la fenêtre 2-30 cm depuis un moment : distance invalide */
      if (maintenant - derniere_mesure[i] > TIMEOUT_MESURE_MS)
      {
        distance[i] = INVALIDE;
      }
    }

    if (maintenant - dernier_envoi >= PERIODE_CAN_MS)
    {
      envoyer_can();
      dernier_envoi = maintenant;
    }
  }
}

void CapteursSommeil_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "CapteursSommeil",
    .stack_size = 1024 * 4,
    .priority = osPriorityNormal
  };
  SemToFHandle = osSemaphoreNew(1, 0, NULL);   /* sémaphore binaire, vide au départ */
  osThreadNew(CapteursSommeil_Tache, NULL, &attr);
}
