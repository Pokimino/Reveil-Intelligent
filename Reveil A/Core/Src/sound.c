/*
 * sound.c
 *
 *  Created on: Nov 3, 2026
 *      Author: simwi
 */


#include "sound.h"
#include "app_msg.h"
#include "main.h"

extern TIM_HandleTypeDef htim8;          /* créé par CubeMX dans main.c */
osMessageQueueId_t Ordres_SonHandle;     /* boîte aux lettres Ordres_Son */

/* Une note = une fréquence en Hz (0 = silence) et une durée en ms */
typedef struct { uint16_t freq; uint16_t duree_ms; } Note_t;

#define DO5  523
#define RE5  587
#define MI5  659
#define FA5  698
#define SOL5 784
#define LA5  880
#define DO6  1047

static const Note_t melodie0[] = {
  {DO5,200},{MI5,200},{SOL5,200},{DO6,400},{0,600}
};
static const Note_t melodie1[] = {
  {SOL5,150},{0,50},{SOL5,150},{0,50},{LA5,300},{SOL5,300},{DO6,300},{0,700}
};
static const Note_t *const melodies[] = { melodie0, melodie1 };
static const uint8_t longueurs[] = {
  sizeof(melodie0) / sizeof(Note_t),
  sizeof(melodie1) / sizeof(Note_t)
};
static volatile uint8_t id_melodie = 0;

void Son_ChoisirMelodie(uint8_t id)
{
  if (id < 2) id_melodie = id;
}

/* Règle le timer sur une fréquence (compteur à 1 MHz) ; freq = 0 : silence */
static void buzzer_jouer(uint16_t freq)
{
  const uint8_t muet = 1;   /* 1 = buzzer coupé, 0 = son normal */

  if (muet || freq == 0)
  {
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, 0);
    return;
  }
  uint32_t arr = 1000000UL / freq - 1;                          /* durée d'un tour -> note */
  __HAL_TIM_SET_AUTORELOAD(&htim8, arr);
  __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_4, (arr + 1) / 2);  /* signal carré 50 % */
}

/* Tâche Son : états SILENCE / MELODIE (diagramme d'états de la conception) */
static void Son_Tache(void *argument)
{
  OrdreSon_t ordre;
  uint8_t active = 0;   /* 0 = SILENCE, 1 = MELODIE */
  uint8_t id = 0, rang = 0;

  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
  buzzer_jouer(0);

  for (;;)
  {
    /* SILENCE : on attend un ordre sans limite ; MELODIE : au plus la durée de la note */
    uint32_t attente = active ? melodies[id][rang].duree_ms : osWaitForever;

    if (osMessageQueueGet(Ordres_SonHandle, &ordre, NULL, attente) == osOK)
    {
      switch (ordre)
      {
        case MELODIE_ON:
          id = id_melodie;
          rang = 0;
          active = 1;
          buzzer_jouer(melodies[id][0].freq);
          break;

        case MELODIE_OFF:
          active = 0;
          buzzer_jouer(0);
          break;

        case BIP:
          buzzer_jouer(2000);
          osDelay(100);
          buzzer_jouer(active ? melodies[id][rang].freq : 0);
          break;
      }
    }
    else if (active)   /* délai écoulé : la note est finie, on passe à la suivante (en boucle) */
    {
      rang = (rang + 1) % longueurs[id];
      buzzer_jouer(melodies[id][rang].freq);
    }
  }
}

void Son_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Son",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  Ordres_SonHandle = osMessageQueueNew(8, sizeof(OrdreSon_t), NULL);
  osThreadNew(Son_Tache, NULL, &attr);
}
