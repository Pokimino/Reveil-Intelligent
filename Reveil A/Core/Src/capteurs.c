/* capteurs.c : tâche Capteurs
 *   Mesure la chambre une fois par seconde et range tout dans Infos :
 *   - température    : TC72 sur SPI2 (CE = PB8, actif à l'état haut)
 *   - qualité d'air  : SGP40 sur I2C1 (librairie du cours), compensé avec la température mesurée
 *   - luminosité     : photorésistance sur PC4 (ADC1 canal 13)
 *   - potentiomètre  : PA6 (ADC1 canal 11), vitesse du ventilateur en mode manuel
 */
#include "capteurs.h"
#include "app_msg.h"
#include "main.h"
#include "SGP40.h"

extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi2;

Infos_t Infos;
osMutexId_t Infos_MutexHandle;
osMutexId_t I2C1_MutexHandle;

#define PERIODE_MS  1000   /* l'algorithme du SGP40 demande une mesure par seconde */
#define HUMIDITE    50     /* pas de capteur d'humidité : valeur par défaut du cours */

/* 1 : sur cette carte, la valeur brute baisse quand la lumière augmente (vérifié) */
static const uint8_t INVERSER = 1;

/* Pour le debug (Live Expressions) */
volatile uint16_t ldr_brut;   /* valeur brute de l'ADC pour la photorésistance, 0..4095 */
volatile uint8_t  tc72_id;    /* doit valoir 0x54 (84) si le SPI fonctionne */
volatile uint8_t  sgp40_ok;   /* 1 si l'auto-test du SGP40 est réussi */


/* ================== TC72 (SPI2) ==================
 * Pendant tout un échange, CE est à 1. Le premier octet envoyé est l'adresse du registre :
 * adresse + 0x80 pour écrire, adresse seule pour lire. */
static void tc72_ecrire(uint8_t adresse, uint8_t valeur)
{
  uint8_t tx[2] = { adresse | 0x80, valeur };

  HAL_GPIO_WritePin(TC72_CE_GPIO_Port, TC72_CE_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit(&hspi2, tx, 2, 10);
  HAL_GPIO_WritePin(TC72_CE_GPIO_Port, TC72_CE_Pin, GPIO_PIN_RESET);
}

/* Lit n octets à partir d'une adresse (le TC72 renvoie l'adresse demandée, puis les adresses inférieures) */
static void tc72_lire(uint8_t adresse, uint8_t *donnees, uint16_t n)
{
  for (uint16_t i = 0; i < n; i++)
  {
    donnees[i] = 0;
  }
  HAL_GPIO_WritePin(TC72_CE_GPIO_Port, TC72_CE_Pin, GPIO_PIN_SET);
  HAL_SPI_Transmit(&hspi2, &adresse, 1, 10);
  HAL_SPI_Receive(&hspi2, donnees, n, 10);
  HAL_GPIO_WritePin(TC72_CE_GPIO_Port, TC72_CE_Pin, GPIO_PIN_RESET);
}

static float tc72_temperature(void)
{
  uint8_t d[3];                 /* MSB, LSB, registre de contrôle */

  tc72_ecrire(0x00, 0x15);      /* registre de contrôle (écrit à 0x80) : une mesure puis veille */
  osDelay(150);                 /* durée d'une conversion */
  tc72_lire(0x02, d, 3);

  /* MSB = partie entière signée, les 2 bits de poids fort du LSB = quarts de degré */
  return (int8_t)d[0] + (d[1] >> 6) * 0.25f;
}


/* ================== ADC1 : photorésistance puis potentiomètre ================== */
static void adc_lire(uint16_t *ldr, uint16_t *pot)
{
  *ldr = 0;
  *pot = 0;
  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    *ldr = HAL_ADC_GetValue(&hadc1);   /* rang 1 : canal 13 */
  }
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    *pot = HAL_ADC_GetValue(&hadc1);   /* rang 2 : canal 11 */
  }
  HAL_ADC_Stop(&hadc1);
}


/* ================== Accès aux mesures pour les autres tâches ================== */
/* Copie de toutes les mesures d'un coup, sous mutex :
 * on ne lit jamais un mélange d'anciennes et de nouvelles valeurs. */
void Infos_Lire(Infos_t *copie)
{
  osMutexAcquire(Infos_MutexHandle, osWaitForever);
  *copie = Infos;
  osMutexRelease(Infos_MutexHandle);
}


/* ================== Tâche ================== */
static void Capteurs_Tache(void *argument)
{
  uint8_t id;
  uint32_t prochain;

  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);   /* étalonnage de l'ADC, une fois */

  tc72_lire(0x03, &id, 1);                                 /* identifiant du TC72 */
  tc72_id = id;

  osMutexAcquire(I2C1_MutexHandle, osWaitForever);         /* l'OLED attend pendant ce temps */
  sgp40_ok = (SGP40_Init() == 0);
  osMutexRelease(I2C1_MutexHandle);

  prochain = osKernelGetTickCount();
  for (;;)
  {
    float T = tc72_temperature();

    osMutexAcquire(I2C1_MutexHandle, osWaitForever);
    uint16_t iqa = SGP40_MeasureVOC(T, HUMIDITE);           /* compensé avec la vraie température */
    osMutexRelease(I2C1_MutexHandle);

    uint16_t brut_ldr, brut_pot;
    adc_lire(&brut_ldr, &brut_pot);
    ldr_brut = brut_ldr;

    uint16_t L = (uint32_t)brut_ldr * 100 / 4095;          /* 0..4095 -> 0..100 */
    if (INVERSER)
    {
      L = 100 - L;
    }

    osMutexAcquire(Infos_MutexHandle, osWaitForever);
    Infos.Tmes = T;
    Infos.IQA  = iqa;
    Infos.L    = L;
    Infos.Vpot = (uint32_t)brut_pot * 100 / 4095;
    osMutexRelease(Infos_MutexHandle);

    /* Exactement une fois par seconde, quel que soit le temps passé dans les mesures */
    prochain += PERIODE_MS;
    osDelayUntil(prochain);
  }
}

void Capteurs_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Capteurs",
    .stack_size = 512 * 4,
    .priority = osPriorityBelowNormal
  };
  Infos_MutexHandle = osMutexNew(NULL);
  I2C1_MutexHandle  = osMutexNew(NULL);
  osThreadNew(Capteurs_Tache, NULL, &attr);
}
