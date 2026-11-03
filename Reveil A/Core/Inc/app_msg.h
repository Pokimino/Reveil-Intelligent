#ifndef APP_MSG_H
#define APP_MSG_H

#include <stdint.h>
#include "cmsis_os.h"

/* Ordres envoyés à la tâche Son (boîte aux lettres Ordres_Son) */
typedef enum { MELODIE_ON, MELODIE_OFF, BIP } OrdreSon_t;
extern osMessageQueueId_t Ordres_SonHandle;

/* Commandes envoyées à la tâche Interface (boîte aux lettres Cmdes)
 * Youssef ajoutera les types qui viennent du téléphone (MAJ_HEURE, MAJ_ALARME...) */
typedef enum { NAV_PREC, NAV_VALID, NAV_SUIV, SIESTE, AUBE_MANUELLE, CREPUSCULE_MANUEL } TypeCmde_t;
typedef struct { TypeCmde_t type; uint32_t valeur; } Cmde_t;
extern osMessageQueueId_t CmdesHandle;

#endif /* APP_MSG_H */
