/*
 * oled.h
 *
 *  Created on: Nov 2, 2026
 *      Author: youssefzaimi
 */


#ifndef OLED_H
#define OLED_H

/*
 * Module OLED - Reveil intelligent
 * Ecran SSD1306 128x64 - I2C1
 */

/* Initialise l'ecran OLED */
void OLED_Init(void);

/* Affiche la page de demarrage */
void OLED_ShowStartup(void);

#endif /* OLED_H */
