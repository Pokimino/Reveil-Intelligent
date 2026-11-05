/* mode_jour_nuit.c : tâche ModeJourNuit
 *   Partie "Mode" de la tâche Reveil de la conception (l'alarme est dans defaultTask, côté Youssef).
 *   - JOUR -> NUIT : L reste sous S_nuit pendant D_validation (+ crépuscule automatique si activé)
 *   - NUIT -> JOUR : L reste au-dessus de S_jour pendant D_validation (+ aube automatique si activée)
 *   - Pas de changement tant qu'une animation lumineuse tourne : sinon les LEDs elles-mêmes
 *     feraient changer le mode. À la fin de l'animation, la condition est réévaluée.
 *   - Le chronomètre repart de zéro dès que la condition sur L devient fausse : c'est ce qui
 *     ignore les variations brèves (main qui passe, ombre...). C'est notre "L filtrée".
 *   - Plages horaires : non utilisées pour la démo (toutes les heures sont autorisées).
 */
#include "mode_jour_nuit.h"
#include "partage.h"
#include "app_msg.h"

#define PERIODE_MS  500   /* L est mesurée toutes les secondes : on regarde 2 fois plus souvent */

/* Pour le debug (Live Expressions) */
volatile Mode_t   mode_debug;
volatile uint32_t chrono_ms;   /* depuis combien de temps la condition de changement est vraie */

static void envoyer_lumiere(OrdreLumiere_t o)
{
  osMessageQueuePut(Ordres_LumiereHandle, &o, 0, 0);
}

static void ModeJourNuit_Tache(void *argument)
{
  Mode_t   mode = MODE_JOUR;
  uint8_t  condition_vraie = 0;
  uint32_t debut_condition = 0;
  Infos_t  in;
  Config_t cfg;

  Mode_Ecrire(mode);
  osDelay(2000);   /* on laisse aux capteurs le temps de donner une vraie valeur de L */

  for (;;)
  {
    Infos_Lire(&in);
    Config_Lire(&cfg);
    uint32_t maintenant = osKernelGetTickCount();

    /* La condition qui ferait changer de mode */
    uint8_t cond = (mode == MODE_JOUR) ? (in.L < cfg.S_nuit) : (in.L > cfg.S_jour);

    if (!cond)
    {
      condition_vraie = 0;                 /* le chronomètre repart de zéro */
    }
    else if (!condition_vraie)
    {
      condition_vraie = 1;
      debut_condition = maintenant;
    }

    chrono_ms = condition_vraie ? (maintenant - debut_condition) : 0;
    uint8_t confirme      = condition_vraie && (chrono_ms >= cfg.D_validation_ms);
    uint8_t leds_eteintes = (Etat_Lumiere == LUMIERE_INACTIVE);

    if (confirme && leds_eteintes)
    {
      if (mode == MODE_JOUR)
      {
        mode = MODE_NUIT;
        if (cfg.crepuscule_actif) envoyer_lumiere(CREPUSCULE_AUTO);
      }
      else
      {
        mode = MODE_JOUR;
        if (cfg.aube_active) envoyer_lumiere(AUBE_AUTO);
      }
      Mode_Ecrire(mode);
      condition_vraie = 0;
    }

    mode_debug = mode;
    osDelay(PERIODE_MS);
  }
}

void ModeJourNuit_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "ModeJourNuit",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  osThreadNew(ModeJourNuit_Tache, NULL, &attr);
}
