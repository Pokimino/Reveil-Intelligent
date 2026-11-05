/* reception_can.c (carte A) : reçoit les distances du sommeil envoyées par les cartes B et C
 *   Interruption CAN (FIFO 0) -> boîte aux lettres TramesCAN -> tâche ReceptionCAN -> Infos.d1..d4
 *
 *   Format des messages (le même que sur les cartes B et C) :
 *     identifiant 0x21 = carte B : d1 (tête),   d2 (tronc)
 *     identifiant 0x22 = carte C : d3 (bassin), d4 (jambes)
 *     4 octets = 2 distances en mm, poids fort en premier, 0xFFFF = mesure invalide
 */
#include "reception_can.h"
#include "app_msg.h"
#include "main.h"
#include <string.h>

extern CAN_HandleTypeDef hcan1;

#define ID_CAN_B          0x21
#define ID_CAN_C          0x22
#define INVALIDE          0xFFFF
#define TIMEOUT_CARTE_MS  1000   /* plus de message d'une carte depuis 1 s : ses distances deviennent invalides */

typedef struct { uint32_t id; uint8_t data[4]; } TrameCAN_t;
static osMessageQueueId_t TramesCANHandle;

/* Pour le debug (Live Expressions) : doit augmenter d'environ 10 par seconde et par carte */
volatile uint32_t nb_trames_can;


/* Interruption : un message est arrivé. On le range dans la boîte aux lettres, rien de plus. */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
  CAN_RxHeaderTypeDef entete;
  uint8_t data[8];
  TrameCAN_t t;

  if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &entete, data) == HAL_OK && entete.DLC >= 4)
  {
    t.id = entete.StdId;
    memcpy(t.data, data, 4);
    osMessageQueuePut(TramesCANHandle, &t, 0, 0);   /* depuis une interruption : jamais d'attente */
  }
}

static void ecrire_distances(uint32_t id, uint16_t da, uint16_t db)
{
  osMutexAcquire(Infos_MutexHandle, osWaitForever);
  if (id == ID_CAN_B)
  {
    Infos.d1 = da;
    Infos.d2 = db;
  }
  else if (id == ID_CAN_C)
  {
    Infos.d3 = da;
    Infos.d4 = db;
  }
  osMutexRelease(Infos_MutexHandle);
}

static void ReceptionCAN_Tache(void *argument)
{
  CAN_FilterTypeDef filtre = {0};
  TrameCAN_t t;
  uint32_t dernier_B, dernier_C;

  /* Filtre du CAN : on laisse passer tous les messages, le tri par identifiant se fait plus bas */
  filtre.FilterBank = 0;
  filtre.FilterMode = CAN_FILTERMODE_IDMASK;
  filtre.FilterScale = CAN_FILTERSCALE_32BIT;
  filtre.FilterIdHigh = 0;
  filtre.FilterIdLow = 0;
  filtre.FilterMaskIdHigh = 0;
  filtre.FilterMaskIdLow = 0;
  filtre.FilterFIFOAssignment = CAN_RX_FIFO0;
  filtre.FilterActivation = CAN_FILTER_ENABLE;
  filtre.SlaveStartFilterBank = 14;
  HAL_CAN_ConfigFilter(&hcan1, &filtre);

  HAL_CAN_Start(&hcan1);
  HAL_CAN_ActivateNotification(&hcan1, CAN_IT_RX_FIFO0_MSG_PENDING);

  ecrire_distances(ID_CAN_B, INVALIDE, INVALIDE);   /* rien reçu pour l'instant */
  ecrire_distances(ID_CAN_C, INVALIDE, INVALIDE);
  dernier_B = dernier_C = osKernelGetTickCount();

  for (;;)
  {
    /* On attend un message, au plus 100 ms (pour surveiller les cartes muettes) */
    if (osMessageQueueGet(TramesCANHandle, &t, NULL, 100) == osOK)
    {
      uint16_t da = ((uint16_t)t.data[0] << 8) | t.data[1];
      uint16_t db = ((uint16_t)t.data[2] << 8) | t.data[3];

      nb_trames_can++;
      ecrire_distances(t.id, da, db);
      if (t.id == ID_CAN_B) dernier_B = osKernelGetTickCount();
      if (t.id == ID_CAN_C) dernier_C = osKernelGetTickCount();
    }

    /* Carte débranchée ou éteinte : ses distances ne sont plus valables */
    uint32_t maintenant = osKernelGetTickCount();
    if (maintenant - dernier_B > TIMEOUT_CARTE_MS) ecrire_distances(ID_CAN_B, INVALIDE, INVALIDE);
    if (maintenant - dernier_C > TIMEOUT_CARTE_MS) ecrire_distances(ID_CAN_C, INVALIDE, INVALIDE);
  }
}

void ReceptionCAN_Init(void)
{
  static const osThreadAttr_t attr = {
    .name = "ReceptionCAN",
    .stack_size = 256 * 4,
    .priority = osPriorityNormal
  };
  TramesCANHandle = osMessageQueueNew(8, sizeof(TrameCAN_t), NULL);
  osThreadNew(ReceptionCAN_Tache, NULL, &attr);
}
