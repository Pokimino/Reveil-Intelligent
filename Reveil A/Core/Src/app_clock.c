/*
 * app_clock.c
 *
 *  Created on: Nov 3, 2026
 *      Author: youssefzaimi
 */



#include "app_clock.h"

/* Signature conservee dans la memoire RTC */
#define APP_CLOCK_MAGIC 0x52455641UL

HAL_StatusTypeDef AppClock_Init(RTC_HandleTypeDef *rtc)
{
    if (rtc == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * Si l'heure a deja ete initialisee,
     * on ne la remet pas a zero.
     */
    if (HAL_RTCEx_BKUPRead(rtc, RTC_BKP_DR0)
            == APP_CLOCK_MAGIC)
    {
        return HAL_OK;
    }

    /* Heure de test : 12:34:50 */
    RTC_TimeTypeDef sTime = {0};

    sTime.Hours = 12;
    sTime.Minutes = 34;
    sTime.Seconds = 50;

    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    if (HAL_RTC_SetTime(rtc, &sTime, RTC_FORMAT_BIN)
            != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Date de test : 03/11/2026 */
    RTC_DateTypeDef sDate = {0};

    sDate.WeekDay = RTC_WEEKDAY_TUESDAY;
    sDate.Month = RTC_MONTH_NOVEMBER;
    sDate.Date = 3;
    sDate.Year = 26;

    if (HAL_RTC_SetDate(rtc, &sDate, RTC_FORMAT_BIN)
            != HAL_OK)
    {
        return HAL_ERROR;
    }

    /* Memorise que l'horloge est configuree */
    HAL_RTCEx_BKUPWrite(
        rtc,
        RTC_BKP_DR0,
        APP_CLOCK_MAGIC
    );

    return HAL_OK;
}

HAL_StatusTypeDef AppClock_Read(
    RTC_HandleTypeDef *rtc,
    AppClock_Time_t *time
)
{
    if (rtc == NULL || time == NULL)
    {
        return HAL_ERROR;
    }

    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    if (HAL_RTC_GetTime(
            rtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /*
     * Important : lire la date apres l'heure
     * pour debloquer les registres RTC.
     */
    if (HAL_RTC_GetDate(
            rtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return HAL_ERROR;
    }

    time->hours = sTime.Hours;
    time->minutes = sTime.Minutes;
    time->seconds = sTime.Seconds;

    return HAL_OK;
}

HAL_StatusTypeDef AppClock_SetTime(
    RTC_HandleTypeDef *rtc,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds)
{
    /* Verification des parametres */
    if (rtc == NULL ||
        hours > 23 ||
        minutes > 59 ||
        seconds > 59)
    {
        return HAL_ERROR;
    }

    RTC_TimeTypeDef sTime = {0};

    sTime.Hours = hours;
    sTime.Minutes = minutes;
    sTime.Seconds = seconds;

    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    return HAL_RTC_SetTime(
        rtc,
        &sTime,
        RTC_FORMAT_BIN
    );
}

