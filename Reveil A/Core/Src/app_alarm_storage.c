
#include "app_alarm_storage.h"

/*
 * Organisation du registre RTC_BKP_DR1 :
 *
 * Bits 31..12 : signature
 * Bit 11      : activation
 * Bits 10..6  : heures
 * Bits 5..0   : minutes
 */

#define ALARM_BKP_MAGIC 0xA14A5000UL
#define ALARM_BKP_MASK  0xFFFFF000UL

HAL_StatusTypeDef AppAlarmStorage_Save(
    RTC_HandleTypeDef *rtc,
    const AppAlarm_t *alarm)
{
    if (rtc == NULL || alarm == NULL)
    {
        return HAL_ERROR;
    }

    if (alarm->hours > 23 ||
        alarm->minutes > 59 ||
        alarm->enabled > 1)
    {
        return HAL_ERROR;
    }

    /* Encoder la configuration */
    uint32_t data =
        ALARM_BKP_MAGIC |
        ((uint32_t)alarm->enabled << 11) |
        ((uint32_t)alarm->hours << 6) |
        (uint32_t)alarm->minutes;

    /* Autoriser l'acces au domaine backup */
    HAL_PWR_EnableBkUpAccess();

    /* Sauvegarder */
    HAL_RTCEx_BKUPWrite(
        rtc,
        RTC_BKP_DR1,
        data
    );

    /* Verifier l'ecriture */
    if (HAL_RTCEx_BKUPRead(
            rtc, RTC_BKP_DR1) != data)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

uint8_t AppAlarmStorage_Load(
    RTC_HandleTypeDef *rtc,
    AppAlarm_t *alarm,
    uint32_t snoozeSeconds)
{
    if (rtc == NULL || alarm == NULL)
    {
        return 0;
    }

    /* Lire la configuration sauvegardee */
    uint32_t data = HAL_RTCEx_BKUPRead(
        rtc,
        RTC_BKP_DR1
    );

    /* Verifier la signature */
    if ((data & ALARM_BKP_MASK)
        != ALARM_BKP_MAGIC)
    {
        return 0;
    }

    uint8_t enabled =
        (uint8_t)((data >> 11) & 0x01);

    uint8_t hours =
        (uint8_t)((data >> 6) & 0x1F);

    uint8_t minutes =
        (uint8_t)(data & 0x3F);

    /* Verifier les valeurs */
    if (hours > 23 || minutes > 59)
    {
        return 0;
    }

    /* Restaurer la configuration */
    AppAlarm_Init(
        alarm,
        hours,
        minutes,
        snoozeSeconds
    );

    if (!enabled)
    {
        AppAlarm_Stop(alarm);
    }

    return 1;
}
