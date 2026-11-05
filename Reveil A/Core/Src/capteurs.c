/* capteurs.c : tâche Capteurs
 *   Mesure régulièrement la chambre et range les valeurs dans Infos.
 *   Pour l'instant : la luminosité (photorésistance sur PC4 = ADC1 canal 13).
 *   Plus tard : la température et la qualité de l'air.
 */
#include "capteurs.h"
#include "app_msg.h"
#include "main.h"

extern ADC_HandleTypeDef hadc1;

Infos_t Infos;
osMutexId_t Infos_MutexHandle;

#define PERIODE_MS  500   /* une mesure toutes les 0,5 s */

/* Si L BAISSE quand tu éclaires la photorésistance, mets 1 ici */
static const uint8_t INVERSER = 1;

/* Pour le debug (Live Expressions) : valeur brute de l'ADC, de 0 à 4095 */
volatile uint16_t ldr_brut;

/* Une conversion de l'ADC sur la photorésistance */
static uint16_t lire_ldr(void)
{
  uint16_t v = 0;

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
  {
    v = HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);
  return v;
}

/* Pour les autres tâches : copie de toutes les mesures d'un coup, sous mutex.
 * Le mutex garantit qu'on ne lit jamais un mélange d'anciennes et de nouvelles valeurs. */
void Infos_Lire(Infos_t *copie)
{
  osMutexAcquire(Infos_MutexHandle, osWaitForever);
  *copie = Infos;
  osMutexRelease(Infos_MutexHandle);
}

static void Capteurs_Tache(void *argument)
{
  HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);   /* étalonnage de l'ADC, une fois */

  for (;;)
  {
    uint16_t brut = lire_ldr();
    uint16_t L = (uint32_t)brut * 100 / 4095;              /* 0..4095 -> 0..100 */
    if (INVERSER)
    {
      L = 100 - L;
    }
    ldr_brut = brut;

    osMutexAcquire(Infos_MutexHandle, osWaitForever);
    Infos.L = L;
    osMutexRelease(Infos_MutexHandle);

    osDelay(PERIODE_MS);
  }
}

void Capteurs_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Capteurs",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  Infos_MutexHandle = osMutexNew(NULL);
  osThreadNew(Capteurs_Tache, NULL, &attr);
}
