/*
 * oled.c
 *
 *  Created on: Nov 2, 2026
 *      Author: youssefzaimi
 */

#include "oled.h"
#include "ssd1306.h"
#include "fonts.h"
#include <stdio.h>

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


void OLED_ShowAlarm(
    uint8_t hours,
    uint8_t minutes,
    uint8_t seconds,
    uint8_t alarmHours,
    uint8_t alarmMinutes,
    AppAlarm_State_t state,
    uint8_t enabled)
{
    char timeText[9];

    /* Texte : AL 07:30 OFF */
    char alarmText[13] = "AL 00:00 OFF";

    /* Conversion de l'heure RTC */
    timeText[0] = '0' + hours / 10;
    timeText[1] = '0' + hours % 10;
    timeText[2] = ':';
    timeText[3] = '0' + minutes / 10;
    timeText[4] = '0' + minutes % 10;
    timeText[5] = ':';
    timeText[6] = '0' + seconds / 10;
    timeText[7] = '0' + seconds % 10;
    timeText[8] = '\0';

    /* Conversion de l'heure programmee */
    alarmText[3] = '0' + alarmHours / 10;
    alarmText[4] = '0' + alarmHours % 10;
    alarmText[6] = '0' + alarmMinutes / 10;
    alarmText[7] = '0' + alarmMinutes % 10;

    /* Affichage ON ou OFF */
    if (enabled)
    {
        alarmText[9] = 'O';
        alarmText[10] = 'N';
        alarmText[11] = '\0';
    }

    /* Effacement */
    SSD1306_Fill(SSD1306_COLOR_BLACK);

    /* Ligne 1 : titre */
    SSD1306_GotoXY(43, 8);
    SSD1306_Puts(
        "REVEIL",
        &Font_7x10,
        SSD1306_COLOR_WHITE
    );

    /* Ligne 2 : heure actuelle */
    SSD1306_GotoXY(20, 19);
    SSD1306_Puts(
        timeText,
        &Font_11x18,
        SSD1306_COLOR_WHITE
    );

    /* Ligne 3 : heure de l'alarme */
    SSD1306_GotoXY(22, 39);
    SSD1306_Puts(
        alarmText,
        &Font_7x10,
        SSD1306_COLOR_WHITE
    );

    /* Ligne 4 : etat */
    if (state == APP_ALARM_SONNERIE)
    {
        SSD1306_GotoXY(36, 52);
        SSD1306_Puts(
            "SONNERIE",
            &Font_7x10,
            SSD1306_COLOR_WHITE
        );
    }
    else if (state == APP_ALARM_SNOOZE)
    {
        SSD1306_GotoXY(43, 52);
        SSD1306_Puts(
            "SNOOZE",
            &Font_7x10,
            SSD1306_COLOR_WHITE
        );
    }
    else if (enabled)
    {
        SSD1306_GotoXY(22, 52);
        SSD1306_Puts(
            "ALARME PRETE",
            &Font_7x10,
            SSD1306_COLOR_WHITE
        );
    }
    else
    {
        SSD1306_GotoXY(29, 52);
        SSD1306_Puts(
            "ALARME OFF",
            &Font_7x10,
            SSD1306_COLOR_WHITE
        );
    }

    /* Mise a jour de l'ecran */
    SSD1306_UpdateScreen();
}


/* ===== Y2 : affichage des differents ecrans ===== */

static void OLED_Texte(uint8_t x, uint8_t y,
                       const char *texte)
{
    SSD1306_GotoXY(x, y);
    SSD1306_Puts((char *)texte,
                 &Font_7x10,
                 SSD1306_COLOR_WHITE);
}

static void OLED_GrandTexte(uint8_t x, uint8_t y,
                            const char *texte)
{
    SSD1306_GotoXY(x, y);
    SSD1306_Puts((char *)texte,
                 &Font_11x18,
                 SSD1306_COLOR_WHITE);
}

void OLED_SetContrast(uint8_t contraste)
{
    /* Commande SSD1306 : regler le contraste */
    ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, 0x81);
    ssd1306_I2C_Write(SSD1306_I2C_ADDR, 0x00, contraste);
}



void OLED_ShowPage(OLED_Page_t page,
                   const OLED_Donnees_t *d)
{
    if (d == NULL || page >= OLED_PAGE_NOMBRE)
        return;




    static const char *titres[] = {
        "HEURE",
        "TEMPERATURE",
        "QUALITE AIR",
        "SOMMEIL",
        "ALARME",
        "SIESTE",
        "CONSIGNE",
        "SEUIL AIR",
        "REGLAGES",
        "VENTILATION"
    };

    char ligne[32];

    SSD1306_Fill(SSD1306_COLOR_BLACK);

    OLED_Texte(4, 2, titres[page]);

    switch (page)
    {
        case OLED_PAGE_HEURE:
            snprintf(ligne, sizeof(ligne),
                     "%02u:%02u:%02u",
                     d->reveil.h,
                     d->reveil.m,
                     d->reveil.s);
            OLED_GrandTexte(17, 22, ligne);

            OLED_Texte(0, 51,
                       d->mode == MODE_NUIT ?
                       "MODE : NUIT" : "MODE : JOUR");
            break;

        case OLED_PAGE_TEMPERATURE:
        {
            int temp = (int)(
                d->infos.Tmes >= 0 ?
                d->infos.Tmes + 0.5f :
                d->infos.Tmes - 0.5f);

            snprintf(ligne, sizeof(ligne),
                     "%d C", temp);
            OLED_GrandTexte(25, 22, ligne);

            snprintf(ligne, sizeof(ligne),
                     "CONSIGNE : %d C",
                     (int)(d->config.Tcons + 0.5f));
            OLED_Texte(0, 51, ligne);
            break;
        }

        case OLED_PAGE_AIR:
            if (d->infos.IQA == 0)
            {
                OLED_Texte(0, 25, "INITIALISATION");
                OLED_Texte(0, 43, "CAPTEUR VOC");
            }
            else
            {
                snprintf(ligne, sizeof(ligne),
                         "IQA %u", d->infos.IQA);
                OLED_GrandTexte(13, 22, ligne);

                OLED_Texte(
                    0, 51,
                    d->infos.IQA > d->config.seuil_IQA ?
                    "AIR A AERER" : "AIR OK");
            }
            break;



        case OLED_PAGE_SOMMEIL:

            /* Score de la derniere nuit */
            if (d->sommeil.Q_nuit == SCORE_AUCUN)
            {
                snprintf(ligne, sizeof(ligne), "--/100");
            }
            else
            {
                snprintf(ligne, sizeof(ligne),
                         "%u/100",
                         (unsigned)d->sommeil.Q_nuit);
            }

            /* Score principal au centre de la zone bleue */
            OLED_GrandTexte(25, 30, ligne);

            /* Etat en bas */
            if (d->sommeil.etat == SOMMEIL_NOCTURNE)
            {
                OLED_Texte(4, 52, "NUIT EN COURS");
            }
            else
            {
                OLED_Texte(4, 52, "HORS SESSION");
            }

            break;



        case OLED_PAGE_ALARME:
            snprintf(ligne, sizeof(ligne),
                     "%02u:%02u",
                     d->reveil.alarme_h,
                     d->reveil.alarme_m);
            OLED_GrandTexte(36, 22, ligne);

            if (d->reveil.etat == APP_ALARM_SONNERIE)
                OLED_Texte(0, 51, "SONNERIE");
            else if (d->reveil.etat == APP_ALARM_SNOOZE)
                OLED_Texte(0, 51, "SNOOZE");
            else if (d->reveil.alarme_active)
                OLED_Texte(0, 51, "ALARME ACTIVE");
            else
                OLED_Texte(0, 51, "ALARME OFF");
            break;



        case OLED_PAGE_SIESTE:

            if (d->sommeil.Q_sieste == SCORE_AUCUN)
            {
                snprintf(ligne, sizeof(ligne), "--/100");
            }
            else
            {
                snprintf(ligne, sizeof(ligne),
                         "%u/100",
                         (unsigned)d->sommeil.Q_sieste);
            }

            OLED_GrandTexte(25, 30, ligne);

            if (d->sommeil.etat == SOMMEIL_SIESTE)
            {
                OLED_Texte(4, 52, "SIESTE EN COURS");
            }
            else if (d->message_sieste == 1)
            {
                OLED_Texte(4, 52, "SIESTE DEMANDEE");
            }
            else if (d->message_sieste == 2)
            {
                OLED_Texte(4, 52, "DEJA DEMANDEE");
            }
            else if (d->message_sieste == 3)
            {
                OLED_Texte(4, 52, "EN ATTENTE...");
            }
            else
            {
                OLED_Texte(4, 52, "APPUI LONG PAD 2");
            }

            break;



        case OLED_PAGE_CONSIGNE:
            snprintf(ligne, sizeof(ligne),
                     "%d C",
                     (int)(d->config.Tcons + 0.5f));
            OLED_GrandTexte(25, 22, ligne);
            OLED_Texte(0, 51, "REGLAGE VIA BT");
            break;

        case OLED_PAGE_SEUIL_AIR:
            snprintf(ligne, sizeof(ligne),
                     "IQA %u", d->config.seuil_IQA);
            OLED_GrandTexte(13, 22, ligne);
            OLED_Texte(0, 51, "REGLAGE VIA BT");
            break;




        case OLED_PAGE_REGLAGES:

            /* Melodie sur une ligne qui s'affiche correctement */
            snprintf(ligne, sizeof(ligne),
                     "MELODIE : %u",
                     (unsigned)d->config.id_melodie);

            OLED_Texte(4, 41, ligne);

            /* Regrouper les deux seuils sur une seule ligne */
            snprintf(ligne, sizeof(ligne),
                     "JOUR:%u  NUIT:%u",
                     (unsigned)d->config.S_jour,
                     (unsigned)d->config.S_nuit);

            OLED_Texte(4, 53, ligne);

            break;






        case OLED_PAGE_VENTILATION:

            /* Mettre le mode en valeur */
            if (d->vent_manuel)
            {
                OLED_GrandTexte(31, 24, "MANUEL");
            }
            else
            {
                OLED_GrandTexte(42, 24, "AUTO");
            }

            /* Valeur secondaire du potentiometre */
            snprintf(ligne, sizeof(ligne),
                     "POT : %u %%",
                     (unsigned)d->infos.Vpot);

            OLED_Texte(4, 43, ligne);

            /* Indication de commande */
            OLED_Texte(4, 53, "PAD 2 : CHANGER");

            break;



        default:
            break;
    }

    SSD1306_UpdateScreen();
}





