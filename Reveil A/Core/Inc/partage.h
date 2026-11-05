/* partage.h : le "contrat" entre les tâches de Simon et celles de Youssef.
 *
 * Toutes les données échangées entre vos deux parties passent par ici.
 * Règle : chaque donnée a UN SEUL écrivain (indiqué), tout le monde peut la lire.
 * On lit/écrit toujours par les fonctions ci-dessous : le mutex est à l'intérieur.
 * Tant que l'écrivain n'a pas codé sa partie, les lecteurs voient les valeurs par défaut
 * (voir partage.c) : chacun peut donc avancer et tester sans attendre l'autre.
 *
 * (Déjà existant ailleurs, dans app_msg.h : Infos, Etat_Lumiere, Vent_Manuel, Cmdes,
 *  Ordres_Son, Ordres_Lumiere, I2C1_MutexHandle)
 */
#ifndef PARTAGE_H
#define PARTAGE_H

#include <stdint.h>
#include "cmsis_os.h"
#include "app_alarm.h"   /* AppAlarm_State_t : SILENCIEUSE / SONNERIE / SNOOZE (code de Youssef) */

/* ---------- Config : réglages ----------
 * Écrit par : Gestion_commandes (Youssef, commandes UART / Bluetooth)
 * Lu par    : Son, Eclairage, Ventilation, ModeJourNuit (Simon) ; Interface (Youssef) */
typedef struct {
  uint8_t  id_melodie;        /* mélodie d'alarme : 0 ou 1 */
  float    Tcons;             /* consigne de température (°C) */
  uint16_t seuil_IQA;         /* seuil de qualité d'air (indice VOC, 100 = moyenne) */
  uint32_t D_aube_ms;         /* durée de la montée bleue */
  uint32_t D_palier_ms;       /* durée du palier bleu */
  uint32_t D_crepuscule_ms;   /* durée de la descente orange */
  uint8_t  aube_active;       /* 1 = aube automatique au passage en JOUR */
  uint8_t  crepuscule_actif;  /* 1 = crépuscule automatique au passage en NUIT */
  uint8_t  S_jour;            /* luminosité L (0..100) au-dessus de laquelle il fait jour */
  uint8_t  S_nuit;            /* luminosité L en dessous de laquelle il fait nuit (S_nuit < S_jour) */
  uint32_t D_validation_ms;   /* durée pendant laquelle L doit rester franche avant de changer de mode */
} Config_t;
void Config_Lire(Config_t *c);
void Config_Ecrire(const Config_t *c);

/* ---------- Mode jour / nuit ----------
 * Écrit par : ModeJourNuit (Simon)
 * Lu par    : Sommeil (Simon) ; Interface (Youssef, écran atténué la nuit) */
typedef enum { MODE_JOUR = 0, MODE_NUIT } Mode_t;
Mode_t Mode_Lire(void);
void   Mode_Ecrire(Mode_t m);

/* ---------- État du réveil ----------
 * Écrit par : defaultTask (Youssef), à chaque tour de boucle
 * Lu par    : Gestes (Simon : gestes actifs seulement si ça sonne) ; Interface (Youssef) */
typedef struct {
  uint8_t h, m, s;                 /* heure courante */
  uint8_t alarme_h, alarme_m;      /* heure de l'alarme */
  uint8_t alarme_active;           /* 1 = alarme activée */
  AppAlarm_State_t etat;           /* APP_ALARM_SILENCIEUSE / SONNERIE / SNOOZE */
} EtatReveil_t;
void EtatReveil_Lire(EtatReveil_t *e);
void EtatReveil_Ecrire(const EtatReveil_t *e);

/* ---------- État du sommeil ----------
 * Écrit par : Sommeil (Simon)
 * Lu par    : Interface (Youssef : écrans SOMMEIL et SIESTE, messages début/fin de sieste) */
typedef enum { SOMMEIL_INACTIF = 0, SOMMEIL_SIESTE, SOMMEIL_NOCTURNE } SessionSommeil_t;
#define SCORE_AUCUN  255          /* pas encore de score */
typedef struct {
  SessionSommeil_t etat;
  uint8_t Q_nuit;                  /* score de la dernière nuit, 0..100 */
  uint8_t Q_sieste;                /* score de la dernière sieste, 0..100 */
} EtatSommeil_t;
void EtatSommeil_Lire(EtatSommeil_t *e);
void EtatSommeil_Ecrire(const EtatSommeil_t *e);

/* ---------- Événements ---------- */
/* Gestes (Simon) -> Réveil (Youssef).
 * Youssef, dans defaultTask : Geste_t g; while (osMessageQueueGet(Evt_GestesHandle, &g, NULL, 0) == osOK)
 *                             { traiter comme les commandes 's' (SNOOZE) et 'x' (STOP) } */
typedef enum { GESTE_SNOOZE = 0, GESTE_STOP } Geste_t;
extern osMessageQueueId_t Evt_GestesHandle;

/* Interface (Youssef) -> Sommeil (Simon) : touche 2 maintenue 2 s (commande SIESTE reçue dans Cmdes).
 * Youssef : osSemaphoreRelease(Evt_SiesteHandle); */
extern osSemaphoreId_t Evt_SiesteHandle;

/* À appeler en PREMIER dans USER CODE BEGIN RTOS_THREADS */
void Partage_Init(void);

#endif /* PARTAGE_H */
