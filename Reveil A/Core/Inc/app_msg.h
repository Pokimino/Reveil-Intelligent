#ifndef APP_MSG_H
#define APP_MSG_H

#include <stdint.h>
#include "cmsis_os.h"

/* Ordres envoyés à la tâche Son (boîte aux lettres Ordres_Son) */
typedef enum { MELODIE_ON, MELODIE_OFF, BIP } OrdreSon_t;
extern osMessageQueueId_t Ordres_SonHandle;

/* Ordres envoyés à la tâche Eclairage (boîte aux lettres Ordres_Lumiere)
 * AUTO = envoyé par Reveil (passage jour/nuit), MANUEL = bouton SW1 / SW2 */
typedef enum { AUBE_AUTO, AUBE_MANUELLE, CREPUSCULE_AUTO, CREPUSCULE_MANUEL } OrdreLumiere_t;
extern osMessageQueueId_t Ordres_LumiereHandle;

/* Animation en cours : écrite par Eclairage, lue par Reveil (pas de changement jour/nuit pendant une animation) */
typedef enum { LUMIERE_INACTIVE, LUMIERE_AUBE, LUMIERE_CREPUSCULE } EtatLumiere_t;
extern volatile EtatLumiere_t Etat_Lumiere;

/* Commandes envoyées à la tâche Interface (boîte aux lettres Cmdes)
 * Youssef ajoutera les types qui viennent du téléphone (MAJ_HEURE, MAJ_ALARME...) */
typedef enum { NAV_PREC, NAV_VALID, NAV_SUIV, SIESTE } TypeCmde_t;
typedef struct { TypeCmde_t type; uint32_t valeur; } Cmde_t;
extern osMessageQueueId_t CmdesHandle;

/* Mesures de la chambre (Infos) : écrites par la tâche Capteurs,
 * lues par Reveil (L), Ventilation et Interface (Tmes, IQA).
 * Plusieurs tâches y accèdent : on passe par un mutex. Pour lire, appeler Infos_Lire(&copie). */
typedef struct {
  float    Tmes;   /* température mesurée en °C          (à venir) */
  uint16_t IQA;    /* qualité de l'air                    (à venir) */
  uint16_t L;      /* luminosité : 0 = noir, 100 = plein jour */
} Infos_t;
extern Infos_t Infos;
extern osMutexId_t Infos_MutexHandle;
void Infos_Lire(Infos_t *copie);

#endif /* APP_MSG_H */
