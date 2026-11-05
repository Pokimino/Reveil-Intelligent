/* ventilation.c : tâche Ventilation (suit le pseudo-code de la conception)
 *   Moteur : PWM 1 kHz sur PA5 (TIM2_CH1), DIR = PA9 (sens), DIS = PB5 (1 = moteur coupé)
 *   Modes  : ARRET, RAFRAICHIR (vitesse selon Tmes - Tcons), AERER (balayage avant/arrière),
 *            MANUEL (vitesse réglée au potentiomètre)
 */
#include "ventilation.h"
#include "app_msg.h"
#include "main.h"

extern TIM_HandleTypeDef htim2;

volatile uint8_t Vent_Manuel = 0;   /* écrit par Interface (touche 2 sur l'écran VENTILATION) */

/* ---------- Réglages (plus tard : lus dans Config) ---------- */
#define TCONS          24.0f   /* consigne de température (°C) */
#define H_TEMP          0.5f   /* hystérésis en température (°C) */
#define K              20.0f   /* % de vitesse ajouté par °C au-dessus de la consigne */
#define VMAX          100      /* vitesse max (%) */
#define SEUIL_IQA     150      /* seuil de qualité d'air (indice VOC, 100 = moyenne) */
#define H_IQA          20      /* hystérésis en qualité d'air */
#define D_AV_MS     10000      /* aération : durée en marche avant */
#define D_AR_MS      5000      /* aération : durée en marche arrière */
#define T_ANTI_OSC_MS 10000    /* durée mini dans un mode automatique avant d'en changer */
#define PERIODE_MS    200

#define ARR_TIM2    80000      /* 80 MHz / 80000 = 1 kHz, comme dans le TP du cours */

typedef enum { ARRET, RAFRAICHIR, AERER, MANUEL } EtatVent_t;
typedef enum { AV, AR } Sens_t;

volatile EtatVent_t etat_vent;   /* pour le debug (Live Expressions) */

/* Sortie vers le moteur (Actions) : vitesse en %, sens avant ou arrière */
static void moteur(uint8_t vitesse, Sens_t sens)
{
  HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, (sens == AR) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, (vitesse == 0) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)vitesse * ARR_TIM2 / 100);
}

/* Aérer passe avant rafraîchir. L'hystérésis (h) évite d'osciller autour des seuils. */
static EtatVent_t choix_auto(EtatVent_t etat, const Infos_t *in)
{
  if (in->IQA > SEUIL_IQA + H_IQA || (etat == AERER && in->IQA >= SEUIL_IQA - H_IQA))
  {
    return AERER;
  }
  if (in->Tmes > TCONS + H_TEMP || (etat == RAFRAICHIR && in->Tmes >= TCONS - H_TEMP))
  {
    return RAFRAICHIR;
  }
  return ARRET;
}

static void Ventilation_Tache(void *argument)
{
  EtatVent_t etat = ARRET;
  uint32_t debut_etat = osKernelGetTickCount();   /* sert à l'anti-oscillation et à l'alternance */
  Sens_t sens_precedent = AV;
  Infos_t in;

  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  moteur(0, AV);

  for (;;)
  {
    Infos_Lire(&in);
    uint32_t t = osKernelGetTickCount() - debut_etat;
    EtatVent_t nouvel;

    if (Vent_Manuel)                              nouvel = MANUEL;   /* immédiat, prioritaire */
    else if (etat != MANUEL && t < T_ANTI_OSC_MS) nouvel = etat;     /* anti-oscillation */
    else                                          nouvel = choix_auto(etat, &in);

    if (nouvel != etat)
    {
      etat = nouvel;
      debut_etat = osKernelGetTickCount();
      t = 0;
    }

    uint8_t vitesse = 0;
    Sens_t sens = AV;
    switch (etat)
    {
      case ARRET:
        vitesse = 0;
        break;
      case MANUEL:
        vitesse = in.Vpot;
        break;
      case RAFRAICHIR:
      {
        float v = K * (in.Tmes - TCONS);
        if (v < 0)    v = 0;
        if (v > VMAX) v = VMAX;
        vitesse = (uint8_t)v;
        break;
      }
      case AERER:
        vitesse = VMAX;
        sens = (t % (D_AV_MS + D_AR_MS) < D_AV_MS) ? AV : AR;
        break;
    }

    /* Avant d'inverser le sens, on coupe le moteur pendant une période (pas d'à-coup de courant) */
    if (sens != sens_precedent)
    {
      vitesse = 0;
      sens_precedent = sens;
    }

    moteur(vitesse, sens);
    etat_vent = etat;
    osDelay(PERIODE_MS);
  }
}

void Ventilation_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "Ventilation",
    .stack_size = 256 * 4,
    .priority = osPriorityBelowNormal
  };
  osThreadNew(Ventilation_Tache, NULL, &attr);
}
