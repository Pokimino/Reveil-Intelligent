/*
 * app_alarm.c
 *
 *  Created on: Nov 3, 2026
 *      Author: youssefzaimi
 */



#include "app_alarm.h"

#define SECONDS_PER_DAY 86400UL

/* Convertit l'heure en secondes depuis minuit */
static uint32_t SecondsOfDay(
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds)
{
    return (uint32_t)hours * 3600UL
         + (uint32_t)minutes * 60UL
         + seconds;
}

void AppAlarm_Init(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint32_t snoozeSeconds)
{
    if (alarm == 0)
        return;

    alarm->hours = hours;
    alarm->minutes = minutes;

    alarm->enabled = 1;
    alarm->minuteArmed = 1;

    alarm->state = APP_ALARM_SILENCIEUSE;

    alarm->snoozeStart = 0;
    alarm->snoozeDuration = snoozeSeconds;
}

void AppAlarm_Update(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds)
{
    if (alarm == 0 || !alarm->enabled)
        return;

    uint32_t now = SecondsOfDay(
        hours, minutes, seconds
    );

    uint8_t scheduledMinute =
        (hours == alarm->hours &&
         minutes == alarm->minutes);

    /*
     * Rearmement lorsque l'on quitte
     * la minute programmee.
     */
    if (!scheduledMinute)
    {
        alarm->minuteArmed = 1;
    }

    /* Gestion du delai SNOOZE */
    if (alarm->state == APP_ALARM_SNOOZE)
    {
        uint32_t elapsed =
            (now + SECONDS_PER_DAY
                 - alarm->snoozeStart)
                 % SECONDS_PER_DAY;

        if (elapsed >= alarm->snoozeDuration)
        {
            alarm->state = APP_ALARM_SONNERIE;
        }

        return;
    }

    /* Declenchement a l'heure programmee */
    if (alarm->state == APP_ALARM_SILENCIEUSE &&
        scheduledMinute &&
        alarm->minuteArmed)
    {
        alarm->state = APP_ALARM_SONNERIE;
        alarm->minuteArmed = 0;
    }
}

void AppAlarm_Snooze(
    AppAlarm_t *alarm,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds)
{
    if (alarm == 0 || !alarm->enabled)
        return;

    /* SNOOZE uniquement pendant la sonnerie */
    if (alarm->state != APP_ALARM_SONNERIE)
        return;

    alarm->snoozeStart = SecondsOfDay(
        hours, minutes, seconds
    );

    alarm->state = APP_ALARM_SNOOZE;
}

void AppAlarm_Stop(AppAlarm_t *alarm)
{
    if (alarm == 0)
        return;

    /* STOP est aussi possible en SNOOZE */
    alarm->state = APP_ALARM_SILENCIEUSE;
    alarm->enabled = 0;
    alarm->minuteArmed = 0;
}
