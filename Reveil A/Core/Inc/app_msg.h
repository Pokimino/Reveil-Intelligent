/*
 * app_msg.h
 *
 *  Created on: Nov 3, 2026
 *      Author: simwi
 */

#ifndef INC_APP_MSG_H_
#define INC_APP_MSG_H_

#include "cmsis_os.h"

/* Ordres envoyés à la tâche Son (boîte aux lettres Ordres_Son) */
typedef enum { MELODIE_ON, MELODIE_OFF, BIP } OrdreSon_t;
extern osMessageQueueId_t Ordres_SonHandle;

#endif /* INC_APP_MSG_H_ */
