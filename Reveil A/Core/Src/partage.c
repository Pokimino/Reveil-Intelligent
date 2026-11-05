/* partage.c : données partagées entre les tâches (voir partage.h)
 * Un seul mutex protège tout : les accès sont très courts (une copie de structure). */
#include "partage.h"

static osMutexId_t PartageMutex;

osMessageQueueId_t Evt_GestesHandle;
osSemaphoreId_t    Evt_SiesteHandle;

/* ---------- Valeurs par défaut (réglées pour la démo : durées courtes) ---------- */
static Config_t config = {
  .id_melodie       = 0,
  .Tcons            = 24.0f,
  .seuil_IQA        = 150,
  .D_aube_ms        = 20000,
  .D_palier_ms      = 10000,
  .D_crepuscule_ms  = 20000,
  .aube_active      = 1,
  .crepuscule_actif = 1,
  .S_jour           = 40,
  .S_nuit           = 20,
  .D_validation_ms  = 5000
};
static Mode_t        mode = MODE_JOUR;
static EtatReveil_t  etat_reveil;                                   /* tout à 0 au départ */
static EtatSommeil_t etat_sommeil = { SOMMEIL_INACTIF, SCORE_AUCUN, SCORE_AUCUN };

/* ---------- Accès protégés ---------- */
#define PRENDRE()  osMutexAcquire(PartageMutex, osWaitForever)
#define RENDRE()   osMutexRelease(PartageMutex)

void Config_Lire(Config_t *c)                   { PRENDRE(); *c = config;        RENDRE(); }
void Config_Ecrire(const Config_t *c)           { PRENDRE(); config = *c;        RENDRE(); }

Mode_t Mode_Lire(void)                          { Mode_t m; PRENDRE(); m = mode; RENDRE(); return m; }
void   Mode_Ecrire(Mode_t m)                    { PRENDRE(); mode = m;           RENDRE(); }

void EtatReveil_Lire(EtatReveil_t *e)           { PRENDRE(); *e = etat_reveil;   RENDRE(); }
void EtatReveil_Ecrire(const EtatReveil_t *e)   { PRENDRE(); etat_reveil = *e;   RENDRE(); }

void EtatSommeil_Lire(EtatSommeil_t *e)         { PRENDRE(); *e = etat_sommeil;  RENDRE(); }
void EtatSommeil_Ecrire(const EtatSommeil_t *e) { PRENDRE(); etat_sommeil = *e;  RENDRE(); }

void Partage_Init(void)
{
  PartageMutex     = osMutexNew(NULL);
  Evt_GestesHandle = osMessageQueueNew(4, sizeof(Geste_t), NULL);
  Evt_SiesteHandle = osSemaphoreNew(1, 0, NULL);      /* sémaphore binaire, vide au départ */
}
