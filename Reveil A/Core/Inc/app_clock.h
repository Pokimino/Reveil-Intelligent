/*
 * app_clock.h
 *
 *  Created on: Nov 3, 2026
 *      Author: youssefzaimi
 */


#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include "main.h"

/* Heure au format 24 heures */
typedef struct
{
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
} AppClock_Time_t;

/* Initialisation de l'heure si necessaire */
HAL_StatusTypeDef AppClock_Init(RTC_HandleTypeDef *rtc);

/* Lecture de l'heure courante */
HAL_StatusTypeDef AppClock_Read(
    RTC_HandleTypeDef *rtc,
    AppClock_Time_t *time
);
/* Modification de l'heure RTC */
HAL_StatusTypeDef AppClock_SetTime(
    RTC_HandleTypeDef *rtc,
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds
);

#endif
 /* INC_APP_CLOCK_H_ */
