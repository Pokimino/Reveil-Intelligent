/*
 * sound.h
 *
 *  Created on: Nov 3, 2026
 *      Author: simwi
 */

#ifndef INC_SOUND_H_
#define INC_SOUND_H_

#include <stdint.h>

void Son_Init(void);                  /* crée la file Ordres_Son et la tâche Son */
void Son_ChoisirMelodie(uint8_t id);  /* provisoire : sera lu dans Config.id_melodie */



#endif /* INC_SOUND_H_ */
