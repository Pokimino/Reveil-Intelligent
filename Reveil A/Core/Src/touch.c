/* touch.c : les 3 touches capacitives (périphérique TSC)
 *   Pad 1 (PB12) = précédent -> NAV_PREC
 *   Pad 2 (PB13) = valider   -> NAV_VALID (appui court) ou SIESTE (maintenu 2 s)
 *   Pad 3 (PB14) = suivant   -> NAV_SUIV
 *   Condensateur d'échantillonnage sur PB15
 * Les commandes partent dans la boîte aux lettres Cmdes, lue par la tâche Interface.
 */
#include "touch.h"
#include "app_msg.h"
#include "main.h"

extern TSC_HandleTypeDef htsc;

osMessageQueueId_t CmdesHandle;

#define NB_PADS        3
#define PERIODE_MS     20     /* on mesure les pads toutes les 20 ms */
#define APPUI_LONG_MS  2000   /* pad 2 maintenu 2 s = sieste */

/* Pour le debug : à regarder dans Live Expressions */
volatile uint32_t   touche_mesure[NB_PADS];   /* dernière mesure de chaque pad */
volatile TypeCmde_t derniere_cmde;            /* dernière commande envoyée */
volatile uint32_t   nb_cmdes;                 /* nombre de commandes envoyées */

static const uint32_t canal[NB_PADS] = { TSC_GROUP1_IO1, TSC_GROUP1_IO2, TSC_GROUP1_IO3 };

/* Mesure un pad. Renvoie environ 500 au repos, moins quand on touche, 0 si la mesure rate. */
static uint32_t mesurer(int i)
{
  TSC_IOConfigTypeDef config;

  HAL_TSC_IODischarge(&htsc, ENABLE);    /* on vide les condensateurs */
  osDelay(1);

  config.ChannelIOs  = canal[i];         /* un seul pad à la fois */
  config.SamplingIOs = TSC_GROUP1_IO4;   /* condensateur sur PB15 */
  config.ShieldIOs   = 0;
  HAL_TSC_IOConfig(&htsc, &config);

  HAL_TSC_Start(&htsc);
  HAL_TSC_PollForAcquisition(&htsc);

  if (HAL_TSC_GroupGetStatus(&htsc, TSC_GROUP1_IDX) != TSC_GROUP_COMPLETED)
  {
    return 0;
  }
  return HAL_TSC_GroupGetValue(&htsc, TSC_GROUP1_IDX);
}

static void envoyer(TypeCmde_t type)
{
  Cmde_t c = { type, 0 };
  osMessageQueuePut(CmdesHandle, &c, 0, 0);   /* boîte pleine (personne ne lit encore) : commande perdue, pas grave */
  derniere_cmde = type;
  nb_cmdes++;
}

static void Touches_Tache(void *argument)
{
  uint32_t repos[NB_PADS];          /* valeur de chaque pad sans doigt dessus */
  uint8_t  touche[NB_PADS] = {0};   /* 1 = doigt posé */
  uint32_t debut_pad2 = 0;          /* instant où le doigt s'est posé sur le pad 2 */
  uint8_t  sieste_envoyee = 0;

  /* Au démarrage (sans doigt sur les pads) : on mesure la valeur de repos de chaque pad */
  for (int i = 0; i < NB_PADS; i++)
  {
    uint32_t somme = 0;
    for (int k = 0; k < 8; k++)
    {
      somme += mesurer(i);
    }
    repos[i] = somme / 8;
  }

  for (;;)
  {
    uint8_t vient_appuye[NB_PADS] = {0};
    uint8_t vient_relache[NB_PADS] = {0};

    for (int i = 0; i < NB_PADS; i++)
    {
      uint32_t m = mesurer(i);
      touche_mesure[i] = m;

      /* Doigt posé : la mesure baisse de plus de 20 %.
       * Doigt retiré : elle remonte au-dessus de -10 %.
       * L'écart entre les deux seuils évite les faux appuis (anti-rebond). */
      if (!touche[i] && m > 0 && m < repos[i] * 8 / 10)
      {
        touche[i] = 1;
        vient_appuye[i] = 1;
      }
      else if (touche[i] && m > repos[i] * 9 / 10)
      {
        touche[i] = 0;
        vient_relache[i] = 1;
      }
    }

    /* Pads 1 et 3 : on réagit dès que le doigt se pose */
    if (vient_appuye[0]) envoyer(NAV_PREC);
    if (vient_appuye[2]) envoyer(NAV_SUIV);

    /* Pad 2 : appui court = valider (au relâchement), maintenu 2 s = sieste */
    if (vient_appuye[1])
    {
      debut_pad2 = osKernelGetTickCount();
      sieste_envoyee = 0;
    }
    if (touche[1] && !sieste_envoyee && osKernelGetTickCount() - debut_pad2 >= APPUI_LONG_MS)
    {
      envoyer(SIESTE);
      sieste_envoyee = 1;
    }
    if (vient_relache[1] && !sieste_envoyee)
    {
      envoyer(NAV_VALID);
    }

    osDelay(PERIODE_MS);
  }
}

void Touches_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Touches",
    .stack_size = 256 * 4,
    .priority = osPriorityLow
  };
  CmdesHandle = osMessageQueueNew(8, sizeof(Cmde_t), NULL);   /* boîte aux lettres Cmdes */
  osThreadNew(Touches_Tache, NULL, &attr);
}
