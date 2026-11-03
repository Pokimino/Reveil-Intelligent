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


void OLED_ShowTime(
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds)
{
    char text[9];

    /* Conversion de HH:MM:SS en caracteres */
    text[0] = '0' + hours / 10;
    text[1] = '0' + hours % 10;
    text[2] = ':';
    text[3] = '0' + minutes / 10;
    text[4] = '0' + minutes % 10;
    text[5] = ':';
    text[6] = '0' + seconds / 10;
    text[7] = '0' + seconds % 10;
    text[8] = '\0';

    /* Effacer l'ancien affichage */
    SSD1306_Fill(SSD1306_COLOR_BLACK);

    /* Titre */
    SSD1306_GotoXY(0, 0);
    SSD1306_Puts(
        "REVEIL INTELLIGENT",
        &Font_7x10,
        1
    );

    /* Heure */
    SSD1306_GotoXY(20, 22);
    SSD1306_Puts(
        text,
        &Font_11x18,
        1
    );

    /* Indication */
    SSD1306_GotoXY(22, 48);
    SSD1306_Puts(
        "HORLOGE RTC",
        &Font_7x10,
        1
    );

    /* Mise a jour physique de l'OLED */
    SSD1306_UpdateScreen();
}



