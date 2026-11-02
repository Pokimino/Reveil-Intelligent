/*
 * oled.c
 *
 *  Created on: Nov 2, 2026
 *      Author: youssefzaimi
 */

#include "oled.h"
#include "ssd1306.h"
#include "fonts.h"

/*
 * Initialisation de l'ecran SSD1306.
 * A appeler une seule fois au demarrage.
 */
void OLED_Init(void)
{
    SSD1306_Init();
}

/*
 * Page d'accueil du reveil intelligent.
 * Resolution : 128 x 64 pixels.
 */
void OLED_ShowStartup(void)
{
    /* Premiere ligne */
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts("REVEIL", &Font_11x18, 1);

    /* Deuxieme ligne */
    SSD1306_GotoXY(0, 23);
    SSD1306_Puts("INTELLIGENT", &Font_11x18, 1);

    /* Troisieme ligne */
    SSD1306_GotoXY(0, 48);
    SSD1306_Puts("SYSTEME PRET", &Font_7x10, 1);

    /* Transfert du buffer vers l'ecran */
    SSD1306_UpdateScreen();
}


