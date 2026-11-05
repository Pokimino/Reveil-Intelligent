
#ifndef APP_ALARM_STORAGE_H
#define APP_ALARM_STORAGE_H

#include "main.h"
#include "app_alarm.h"

/* Sauvegarder la configuration de l'alarme */
HAL_StatusTypeDef AppAlarmStorage_Save(
    RTC_HandleTypeDef *rtc,
    const AppAlarm_t *alarm
);

/* Charger la configuration sauvegardee */
/* Retourne 1 si valide, sinon 0 */
uint8_t AppAlarmStorage_Load(
    RTC_HandleTypeDef *rtc,
    AppAlarm_t *alarm,
    uint32_t snoozeSeconds
);

#endif
