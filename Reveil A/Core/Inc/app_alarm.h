/*
 * app_alarm.h
 *
 *  Created on: Nov 3, 2026
 *      Author: youssefzaimi
 */

#ifndef APP_ALARM_H
#define APP_ALARM_H

#include <stdint.h>

typedef enum
{
    APP_ALARM_SILENCIEUSE = 0,
    APP_ALARM_SONNERIE,
    APP_ALARM_SNOOZE

} AppAlarm_State_t;

typedef struct
{
    uint8_t hours;
    uint8_t minutes;

    uint8_t enabled;
    uint8_t minuteArmed;

    AppAlarm_State_t state;

    uint32_t snoozeStart;
    uint32_t snoozeDuration;

} AppAlarm_t;

/* Configuration de l'alarme */
void AppAlarm_Init(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint32_t snoozeSeconds
);

/* Mise a jour periodique depuis le RTC */
void AppAlarm_Update(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds
);

/* Reporter l'alarme */
void AppAlarm_Snooze(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds
);

/* Arreter et desactiver l'alarme */
void AppAlarm_Stop(AppAlarm_t *alarm);

#endif
 /* INC_APP_ALARM_H_ */
