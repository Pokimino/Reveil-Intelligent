/* sommeil.c : tâche Sommeil (suit le pseudo-code de la conception)
 *   Entrées : d1..d4 (ToF des cartes B et C, reçus par CAN, 0xFFFF = invalide), Mode, Evt_Sieste
 *   Sortie  : Etat_Sommeil = { session en cours, score de la dernière nuit, de la dernière sieste }
 *
 *   Sessions :
 *     - NOCTURNE : démarre au passage en NUIT, se termine au passage en JOUR -> Q_nuit
 *     - SIESTE   : appui long sur la touche 2 en mode JOUR (bip), nouvel appui long pour finir (bip)
 *                  -> Q_sieste ; si la nuit tombe pendant la sieste, la session continue en NOCTURNE
 *   Mouvement : une zone dont la distance s'écarte de sa référence d'au moins S_MM pendant au moins
 *               TVAL_MS. Les détections à moins de DT_MS d'intervalle comptent pour un seul mouvement.
 *   Épisode d'agitation : au moins N_EP mouvements en DTA_MS.
 *   Score : Q = max(0, 100 - 2 x Nm - 5 x Na)
 */
#include "sommeil.h"
#include "partage.h"
#include "app_msg.h"

/* Réglages (plus tard : Config.sommeil) -- choisis pour un test à la main */
#define PERIODE_MS   100     /* les cartes B et C envoient toutes les 100 ms */
#define S_MM          50     /* écart de distance qui compte comme un mouvement (5 cm) */
#define TVAL_MS      300     /* l'écart doit durer au moins 0,3 s */
#define DT_MS       1000     /* deux détections à moins d'1 s = un seul mouvement */
#define DTA_MS     10000     /* fenêtre d'agitation : 10 s ... */
#define N_EP           3     /* ... avec au moins 3 mouvements */
#define NB_ZONES       4     /* tête, tronc, bassin, jambes */
#define INVALIDE  0xFFFF

/* Données de la session en cours */
static uint16_t ref[NB_ZONES];             /* distance de référence de chaque zone */
static uint8_t  ecart_en_cours[NB_ZONES];  /* 1 si la zone est écartée de sa référence */
static uint32_t debut_ecart[NB_ZONES];     /* depuis quand */
static uint32_t Nm;                        /* nombre de mouvements */
static uint32_t Na;                        /* nombre d'épisodes d'agitation */
static uint8_t  episode;                   /* 1 pendant un épisode d'agitation */
static uint32_t dernier_mvt;
static uint32_t derniers_mvts[N_EP];       /* heures des N_EP derniers mouvements (tampon tournant) */
static uint8_t  idx_mvt;

/* Pour le debug (Live Expressions) */
volatile SessionSommeil_t session_sommeil;   /* session en cours */
volatile uint8_t  Q_nuit_debug = SCORE_AUCUN, Q_sieste_debug = SCORE_AUCUN;   /* derniers scores */
volatile uint32_t nb_mouvements, nb_episodes;
volatile uint8_t  score_courant;           /* score qu'aurait la session si elle finissait maintenant */
volatile uint8_t  sieste_test;             /* mettre à 1 = comme un appui long (en attendant l'Interface) */


static void bip(void)
{
  OrdreSon_t o = BIP;
  osMessageQueuePut(Ordres_SonHandle, &o, 0, 0);
}

static void nouvelle_session(const uint16_t d[NB_ZONES])
{
  Nm = 0;
  Na = 0;
  episode = 0;
  idx_mvt = 0;
  for (int i = 0; i < NB_ZONES; i++)
  {
    ref[i] = d[i];                   /* la position actuelle devient la référence */
    ecart_en_cours[i] = 0;
  }
}

static uint8_t score(void)
{
  int32_t q = 100 - 2 * (int32_t)Nm - 5 * (int32_t)Na;
  return (q < 0) ? 0 : (uint8_t)q;
}

/* Au moins N_EP mouvements dans les DTA_MS dernières millisecondes ? */
static uint8_t agitation(uint32_t maintenant)
{
  if (Nm < N_EP) return 0;
  uint32_t plus_ancien = derniers_mvts[idx_mvt];   /* le plus ancien des N_EP derniers mouvements */
  return (maintenant - plus_ancien) <= DTA_MS;
}

static void compter_mouvements(const uint16_t d[NB_ZONES], uint32_t maintenant)
{
  uint8_t mouvement = 0;

  for (int i = 0; i < NB_ZONES; i++)
  {
    if (d[i] == INVALIDE)                /* mesure invalide : écartée */
    {
      ecart_en_cours[i] = 0;
      continue;
    }
    if (ref[i] == INVALIDE)              /* première mesure valable de la zone : elle devient la référence */
    {
      ref[i] = d[i];
      continue;
    }

    uint16_t ecart = (d[i] > ref[i]) ? (d[i] - ref[i]) : (ref[i] - d[i]);
    if (ecart >= S_MM)
    {
      if (!ecart_en_cours[i])
      {
        ecart_en_cours[i] = 1;
        debut_ecart[i] = maintenant;
      }
      else if (maintenant - debut_ecart[i] >= TVAL_MS)   /* écart tenu assez longtemps : mouvement */
      {
        mouvement = 1;
        ref[i] = d[i];                   /* la nouvelle position devient la référence */
        ecart_en_cours[i] = 0;
      }
    }
    else
    {
      ecart_en_cours[i] = 0;
    }
  }

  if (mouvement && (Nm == 0 || maintenant - dernier_mvt > DT_MS))   /* détections proches regroupées */
  {
    Nm++;
    dernier_mvt = maintenant;
    derniers_mvts[idx_mvt] = maintenant;
    idx_mvt = (idx_mvt + 1) % N_EP;
    if (agitation(maintenant) && !episode)
    {
      Na++;
      episode = 1;
    }
  }
  if (!agitation(maintenant))
  {
    episode = 0;
  }
}

static void Sommeil_Tache(void *argument)
{
  SessionSommeil_t etat = SOMMEIL_INACTIF;
  uint8_t Q_nuit = SCORE_AUCUN;
  uint8_t Q_sieste = SCORE_AUCUN;
  Infos_t in;

  for (;;)
  {
    Infos_Lire(&in);
    uint16_t d[NB_ZONES] = { in.d1, in.d2, in.d3, in.d4 };
    Mode_t mode = Mode_Lire();
    uint32_t maintenant = osKernelGetTickCount();

    /* Evt_Sieste reçu ? (on le lit sans attendre, puis il est effacé) */
    uint8_t sieste = (osSemaphoreAcquire(Evt_SiesteHandle, 0) == osOK);
    if (sieste_test)
    {
      sieste = 1;
      sieste_test = 0;
    }

    switch (etat)
    {
      case SOMMEIL_INACTIF:
        if (sieste && mode == MODE_JOUR)
        {
          nouvelle_session(d);
          bip();
          etat = SOMMEIL_SIESTE;
        }
        else if (mode == MODE_NUIT)
        {
          nouvelle_session(d);
          etat = SOMMEIL_NOCTURNE;
        }
        break;

      case SOMMEIL_SIESTE:
        compter_mouvements(d, maintenant);
        if (sieste)                      /* nouvel appui long : fin de sieste */
        {
          Q_sieste = score();
          bip();
          etat = SOMMEIL_INACTIF;
        }
        else if (mode == MODE_NUIT)      /* la nuit tombe : la session continue */
        {
          etat = SOMMEIL_NOCTURNE;
        }
        break;

      case SOMMEIL_NOCTURNE:
        compter_mouvements(d, maintenant);
        if (mode == MODE_JOUR)           /* fin de nuit */
        {
          Q_nuit = score();
          etat = SOMMEIL_INACTIF;
        }
        break;
    }

    EtatSommeil_t e = { etat, Q_nuit, Q_sieste };
    EtatSommeil_Ecrire(&e);

    session_sommeil = etat;
    Q_nuit_debug = Q_nuit;
    Q_sieste_debug = Q_sieste;
    nb_mouvements = Nm;
    nb_episodes = Na;
    score_courant = score();

    osDelay(PERIODE_MS);
  }
}

void Sommeil_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Sommeil",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  osThreadNew(Sommeil_Tache, NULL, &attr);
}
