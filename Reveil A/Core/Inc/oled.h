
#ifndef OLED_H
#define OLED_H

#include <stdint.h>
#include "app_alarm.h"

/* Initialisation de l'ecran OLED */
void OLED_Init(void);

/* Affichage de la page de demarrage */
void OLED_ShowStartup(void);

/* Affichage de l'heure RTC */
void OLED_ShowTime(
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds
);


void OLED_ShowAlarm(
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds,
    uint8_t alarmHours,
    uint8_t alarmMinutes,
    AppAlarm_State_t state,
    uint8_t enabled
);

#endif /* OLED_H */
