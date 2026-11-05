/* lights.c : tâche Eclairage (aube bleue et crépuscule orange)
 *   + pilotage des 4 LEDs adressables D1..D4 (fil de données sur PB1 = TIM3_CH4)
 */
#include "lights.h"
#include "app_msg.h"
#include "main.h"

extern TIM_HandleTypeDef htim3;

osMessageQueueId_t Ordres_LumiereHandle;
volatile EtatLumiere_t Etat_Lumiere = LUMIERE_INACTIVE;

/* ---------- Réglages (plus tard : lus dans Config) ---------- */
#define D_AUBE_MS        20000   /* durée de la montée bleue (courte pour le test) */
#define D_PALIER_MS      10000   /* durée du palier bleu à IMAX */
#define D_CREPUSCULE_MS  20000   /* durée de la descente orange */
#define IMAX             150     /* intensité maximale, de 0 à 255 */
#define PAS_RAMPE_MS     50      /* les LEDs sont mises à jour toutes les 50 ms */


/* ================== Pilotage des LEDs (type WS2812) ==================
 * Chaque LED attend 24 bits (8 vert, 8 rouge, 8 bleu), envoyés sur un seul fil.
 * Un bit = une période de PWM de 1,25 us (100 coups d'horloge à 80 MHz) :
 *   bit à 1 = signal haut pendant 0,7 us -> 56
 *   bit à 0 = signal haut pendant 0,3 us -> 24
 *   (valeurs acceptées par les 2 familles de LEDs WS2812B et SK6812, avec un maximum de marge)
 * Le DMA recopie le tableau dans le timer, une valeur par période, sans occuper le processeur.
 * Les 4 LEDs sont branchées en chaîne : on envoie 4 x 24 bits à la suite. */
#define NB_LEDS   4
#define BIT_1     56
#define BIT_0     24
#define NB_DEBUT  3    /* 3 périodes à 0 au début : si le DMA rate la toute première valeur,
                          c'est un de ces 0 qui est perdu, pas un bit de couleur */
#define NB_RESET  50   /* 50 périodes à 0 à la fin : les LEDs comprennent que la trame est finie */

static uint16_t trame[NB_DEBUT + NB_LEDS * 24 + NB_RESET];

/* Met les 4 LEDs à la même couleur (rouge, vert, bleu : de 0 à 255 chacun) */
static void leds_afficher(uint8_t r, uint8_t g, uint8_t b)
{
  uint32_t couleur = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;   /* ordre attendu : vert, rouge, bleu */
  int k = 0;

  for (int i = 0; i < NB_DEBUT; i++)
  {
    trame[k++] = 0;
  }
  for (int led = 0; led < NB_LEDS; led++)
  {
    for (int bit = 23; bit >= 0; bit--)          /* bit de poids fort en premier */
    {
      trame[k++] = (couleur & (1UL << bit)) ? BIT_1 : BIT_0;
    }
  }
  for (int i = 0; i < NB_RESET; i++)
  {
    trame[k++] = 0;
  }

  HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t *)trame, k);
}


/* ================== Tâche Eclairage (suit le pseudo-code de la conception) ================== */
typedef enum { INACTIVE, AUBE_MONTEE, AUBE_PALIER, CREPUSCULE } Etat_t;

static void Eclairage_Tache(void *argument)
{
  Etat_t   etat = INACTIVE;
  uint8_t  origine_manuelle = 0;   /* 1 si l'animation en cours a été lancée par un bouton */
  uint32_t debut = 0;              /* instant de début de l'animation (ms) */
  OrdreLumiere_t o;

  leds_afficher(0, 0, 0);

  for (;;)
  {
    /* Rien en cours : on attend un ordre. Animation en cours : on attend au plus un pas de rampe. */
    uint32_t attente = (etat == INACTIVE) ? osWaitForever : PAS_RAMPE_MS;

    if (osMessageQueueGet(Ordres_LumiereHandle, &o, NULL, attente) == osOK)
    {
      uint8_t manuel    = (o == AUBE_MANUELLE || o == CREPUSCULE_MANUEL);
      uint8_t aube      = (o == AUBE_AUTO || o == AUBE_MANUELLE);
      uint8_t meme_type = (aube && (etat == AUBE_MONTEE || etat == AUBE_PALIER))
                       || (!aube && etat == CREPUSCULE);

      if (manuel && origine_manuelle && meme_type)
      {
        etat = INACTIVE;                         /* annulation : rappui sur le même bouton */
      }
      else if (manuel || etat == INACTIVE || !origine_manuelle)
      {
        origine_manuelle = manuel;               /* on lance (ou remplace) l'animation */
        debut = osKernelGetTickCount();
        etat = aube ? AUBE_MONTEE : CREPUSCULE;
      }
      /* sinon : ordre automatique ignoré, une animation lancée à la main est en cours */
    }

    uint32_t t = osKernelGetTickCount() - debut;

    /* Fins d'animation */
    if (etat == AUBE_MONTEE && t >= D_AUBE_MS)               etat = AUBE_PALIER;
    if (etat == AUBE_PALIER && t >= D_AUBE_MS + D_PALIER_MS) etat = INACTIVE;
    if (etat == CREPUSCULE  && t >= D_CREPUSCULE_MS)         etat = INACTIVE;

    /* Sortie vers les LEDs */
    uint32_t i;
    switch (etat)
    {
      case INACTIVE:
        leds_afficher(0, 0, 0);
        break;
      case AUBE_MONTEE:                          /* bleu de 0 à IMAX */
        i = IMAX * t / D_AUBE_MS;
        leds_afficher(0, 0, i);
        break;
      case AUBE_PALIER:                          /* bleu à IMAX */
        leds_afficher(0, 0, IMAX);
        break;
      case CREPUSCULE:                           /* orange de IMAX à 0 */
        i = IMAX - IMAX * t / D_CREPUSCULE_MS;
        leds_afficher(i, i * 30 / 100, 0);       /* orange = beaucoup de rouge, un peu de vert */
        break;
    }

    if (etat == INACTIVE)        Etat_Lumiere = LUMIERE_INACTIVE;
    else if (etat == CREPUSCULE) Etat_Lumiere = LUMIERE_CREPUSCULE;
    else                         Etat_Lumiere = LUMIERE_AUBE;
  }
}

void Eclairage_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Eclairage",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  Ordres_LumiereHandle = osMessageQueueNew(8, sizeof(OrdreLumiere_t), NULL);
  osThreadNew(Eclairage_Tache, NULL, &attr);
}
