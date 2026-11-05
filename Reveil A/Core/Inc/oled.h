
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


/* ===== Y2 : interface du reveil ===== */
#include "app_msg.h"
#include "partage.h"

typedef enum
{
    OLED_PAGE_HEURE = 0,
    OLED_PAGE_TEMPERATURE,
    OLED_PAGE_AIR,
    OLED_PAGE_SOMMEIL,
    OLED_PAGE_ALARME,
    OLED_PAGE_SIESTE,
    OLED_PAGE_CONSIGNE,
    OLED_PAGE_SEUIL_AIR,
    OLED_PAGE_REGLAGES,
    OLED_PAGE_VENTILATION,
    OLED_PAGE_NOMBRE
} OLED_Page_t;

typedef struct
{
    EtatReveil_t reveil;
    EtatSommeil_t sommeil;
    Infos_t infos;
    Config_t config;
    Mode_t mode;
    uint8_t vent_manuel;
    uint8_t message_sieste;
} OLED_Donnees_t;

/* Toutes ces fonctions sont appelees sous mutex I2C1 */
void OLED_ShowPage(OLED_Page_t page,
                   const OLED_Donnees_t *d);

void OLED_SetContrast(uint8_t contraste);


#endif /* OLED_H */
