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

/* Mesures de la chambre (Infos) : écrites par la tâche Capteurs une fois par seconde,
 * lues par Reveil (L), Ventilation et Interface (Tmes, IQA).
 * Plusieurs tâches y accèdent : on passe par un mutex. Pour lire, appeler Infos_Lire(&copie). */
typedef struct {
  float    Tmes;   /* température mesurée en °C (capteur TC72) */
  uint16_t IQA;    /* qualité de l'air : indice VOC du SGP40, 100 = moyenne de la pièce, plus haut = pire.
                      Vaut 0 pendant les 45 premières secondes (démarrage de l'algorithme). */
  uint16_t L;      /* luminosité : 0 = noir, 100 = plein jour */
  uint16_t Vpot;   /* position du potentiomètre, de 0 à 100 : vitesse du ventilateur en mode manuel */
} Infos_t;
extern Infos_t Infos;
extern osMutexId_t Infos_MutexHandle;
void Infos_Lire(Infos_t *copie);

/* Le bus I2C1 est partagé entre l'écran OLED et le capteur d'air SGP40 :
 * toute utilisation de l'I2C1 doit être encadrée par ce mutex. */
extern osMutexId_t I2C1_MutexHandle;

/* Ventilation manuelle : écrite par Interface (touche 2 sur l'écran VENTILATION), lue par Ventilation */
extern volatile uint8_t Vent_Manuel;

#endif /* APP_MSG_H */
