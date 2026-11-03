/* buttons.c : lecture des 2 boutons physiques
 *   SW1 (PA15) = aube manuelle
 *   SW2 (PB11) = crépuscule manuel
 * Un rappui pendant l'animation lancée par le même bouton l'annule (géré par Eclairage).
 */
#include "buttons.h"
#include "app_msg.h"
#include "main.h"

/* Niveau lu sur la pin quand le bouton est ENFONCÉ (vérifié sur la carte) */
static const GPIO_PinState APPUYE = GPIO_PIN_RESET;

static void envoyer_lumiere(OrdreLumiere_t o)
{
  osMessageQueuePut(Ordres_LumiereHandle, &o, 0, 0);
}

static void Boutons_Tache(void *argument)
{
  uint8_t sw1_avant = 0;
  uint8_t sw2_avant = 0;

  for (;;)
  {
    uint8_t sw1 = (HAL_GPIO_ReadPin(SW1_GPIO_Port, SW1_Pin) == APPUYE);
    uint8_t sw2 = (HAL_GPIO_ReadPin(SW2_GPIO_Port, SW2_Pin) == APPUYE);

    if (sw1 && !sw1_avant)      /* SW1 vient d'être enfoncé */
    {
      envoyer_lumiere(AUBE_MANUELLE);
    }

    if (sw2 && !sw2_avant)      /* SW2 vient d'être enfoncé */
    {
      envoyer_lumiere(CREPUSCULE_MANUEL);
    }

    sw1_avant = sw1;
    sw2_avant = sw2;

    /* On relit les boutons toutes les 20 ms.
     * Les rebonds d'un bouton durent moins de 10 ms, donc on ne les voit pas :
     * c'est l'anti-rebond. */
    osDelay(20);
  }
}

void Boutons_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Boutons",
    .stack_size = 256 * 4,
    .priority = osPriorityNormal
  };
  osThreadNew(Boutons_Tache, NULL, &attr);
}
