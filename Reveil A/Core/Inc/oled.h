
#ifndef OLED_H
#define OLED_H

#include <stdint.h>

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

#endif /* OLED_H */
