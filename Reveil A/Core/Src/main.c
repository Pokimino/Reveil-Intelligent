/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sommeil.h"
#include "oled.h"
#include "app_clock.h"
#include "app_alarm.h"
#include "app_alarm_storage.h"
#include <string.h>
#include "sound.h"
#include "app_msg.h"
#include "buttons.h"
#include "touch.h"
#include "lights.h"
#include "capteurs.h"
#include "ventilation.h"
#include "reception_can.h"
#include "partage.h"
#include "mode_jour_nuit.h"
#include "gestes.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

typedef enum
{
    APP_CMD_SNOOZE = 0,
    APP_CMD_STOP,
    APP_CMD_REARM,
    APP_CMD_SET_TIME,
    APP_CMD_SET_ALARM,
    APP_CMD_SET_ENABLED

} AppCommandType_t;

typedef struct
{
    AppCommandType_t type;

    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t enabled;

} AppCommand_t;

/* Message transmis a la tache OLED */

typedef struct
{
    /* Heure actuelle du RTC */
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;

    /* Heure de l'alarme */
    uint8_t alarmHours;
    uint8_t alarmMinutes;

    /* Etat de l'alarme */
    uint8_t enabled;
    AppAlarm_State_t state;

} DisplayMessage_t;



/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

CAN_HandleTypeDef hcan1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart2;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim8;
DMA_HandleTypeDef hdma_tim3_ch4_up;

TSC_HandleTypeDef htsc;

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for displayTask */
osThreadId_t displayTaskHandle;
const osThreadAttr_t displayTask_attributes = {
  .name = "displayTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for commandTask */
osThreadId_t commandTaskHandle;
const osThreadAttr_t commandTask_attributes = {
  .name = "commandTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for bluetoothTask */
osThreadId_t bluetoothTaskHandle;
const osThreadAttr_t bluetoothTask_attributes = {
  .name = "bluetoothTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE BEGIN PV */
static osMessageQueueId_t displayQueueHandle;
static osMessageQueueId_t commandQueueHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM8_Init(void);
static void MX_TSC_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM2_Init(void);
static void MX_CAN1_Init(void);
static void MX_LPUART1_UART_Init(void);
void StartDefaultTask(void *argument);
void StartDisplayTask(void *argument);
void StartCommandTask(void *argument);
void StartBluetoothTask(void *argument);

/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static uint8_t App_Parse2Digits(
    const char *text,
    uint8_t *value)
{
    if (text[0] < '0' || text[0] > '9' ||
        text[1] < '0' || text[1] > '9')
    {
        return 0;
    }

    *value = (uint8_t)(
        (text[0] - '0') * 10 +
        (text[1] - '0')
    );

    return 1;
}


static void App_UartSend(
    UART_HandleTypeDef *uart,
    const char *text)
{
    HAL_UART_Transmit(
        uart,
        (uint8_t *)text,
        (uint16_t)strlen(text),
        100
    );
}


static uint8_t App_ParseCommand(
    const char *line,
    uint8_t len,
    AppCommand_t *cmd)
{
    uint8_t h = 0;
    uint8_t m = 0;
    uint8_t s = 0;

    /* T HH MM SS */
    if (len == 10 &&
        (line[0] == 'T' || line[0] == 't') &&
        line[1] == ' ' &&
        line[4] == ' ' &&
        line[7] == ' ' &&
        App_Parse2Digits(&line[2], &h) &&
        App_Parse2Digits(&line[5], &m) &&
        App_Parse2Digits(&line[8], &s) &&
        h < 24 && m < 60 && s < 60)
    {
        cmd->type = APP_CMD_SET_TIME;
        cmd->hours = h;
        cmd->minutes = m;
        cmd->seconds = s;

        return 1;
    }

    /* A HH MM */
    if (len == 7 &&
        (line[0] == 'A' || line[0] == 'a') &&
        line[1] == ' ' &&
        line[4] == ' ' &&
        App_Parse2Digits(&line[2], &h) &&
        App_Parse2Digits(&line[5], &m) &&
        h < 24 && m < 60)
    {
        cmd->type = APP_CMD_SET_ALARM;
        cmd->hours = h;
        cmd->minutes = m;

        return 1;
    }

    /* E 0 ou E 1 */
    if (len == 3 &&
        (line[0] == 'E' || line[0] == 'e') &&
        line[1] == ' ' &&
        (line[2] == '0' || line[2] == '1'))
    {
        cmd->type = APP_CMD_SET_ENABLED;
        cmd->enabled = line[2] - '0';

        return 1;
    }

    return 0;
}


static void App_RunTerminal(
    UART_HandleTypeDef *uart,
    uint8_t isBluetooth)
{
    uint8_t rx;

    char line[16];
    uint8_t len = 0;
    uint8_t discardLine = 0;

    if (isBluetooth)
    {
        App_UartSend(
            uart,
            "\r\nBLUETOOTH REVEIL PRET\r\n"
        );
    }
    else
    {
        App_UartSend(
            uart,
            "\r\nREVEIL INTELLIGENT\r\n"
        );
    }

    App_UartSend(
        uart,
        "T HH MM SS = HEURE\r\n"
        "A HH MM = ALARME\r\n"
        "E 1 / E 0 = ON / OFF\r\n"
        "r = REARM\r\n"
        "s = SNOOZE\r\n"
        "x = STOP\r\n"
        "p = TEST\r\n"
    );

    for (;;)
    {
        if (HAL_UART_Receive(
                uart,
                &rx,
                1,
                10) == HAL_OK)
        {
            AppCommand_t cmd = {0};
            uint8_t valid = 0;

            /* Fin de ligne */
            if (rx == '\r' || rx == '\n')
            {
                if (!discardLine && len > 0)
                {
                    valid = App_ParseCommand(
                        line,
                        len,
                        &cmd
                    );

                    if (!valid)
                    {
                        App_UartSend(
                            uart,
                            "ERREUR: commande invalide\r\n"
                        );
                    }
                }

                len = 0;
                discardLine = 0;
            }

            /* Retour arriere */
            else if (!discardLine &&
                     (rx == '\b' || rx == 127))
            {
                if (len > 0)
                {
                    len--;
                }
            }

            /* Commande TEST immediate */
            else if (!discardLine &&
                     len == 0 &&
                     (rx == 'p' || rx == 'P'))
            {
                if (isBluetooth)
                {
                    App_UartSend(uart, "BT OK\r\n");
                }
                else
                {
                    App_UartSend(uart, "UART OK\r\n");
                }
            }

            /* Commandes simples immediates */
            else if (!discardLine &&
                     len == 0 &&
                     (rx == 'r' || rx == 'R' ||
                      rx == 's' || rx == 'S' ||
                      rx == 'x' || rx == 'X'))
            {
                if (rx == 'r' || rx == 'R')
                {
                    cmd.type = APP_CMD_REARM;
                }
                else if (rx == 's' || rx == 'S')
                {
                    cmd.type = APP_CMD_SNOOZE;
                }
                else
                {
                    cmd.type = APP_CMD_STOP;
                }

                valid = 1;
            }

            /* Reception d'une commande longue */
            else if (!discardLine &&
                     rx >= 32 && rx <= 126)
            {
                if (len < sizeof(line))
                {
                    line[len++] = (char)rx;
                }
                else
                {
                    len = 0;
                    discardLine = 1;

                    App_UartSend(
                        uart,
                        "ERREUR: ligne trop longue\r\n"
                    );
                }
            }

            /* Envoi vers defaultTask */
            if (valid)
            {
                if (osMessageQueuePut(
                        commandQueueHandle,
                        &cmd,
                        0,
                        0) == osOK)
                {
                    App_UartSend(
                        uart,
                        "Commande recue\r\n"
                    );
                }
                else
                {
                    App_UartSend(
                        uart,
                        "ERREUR: queue pleine\r\n"
                    );
                }
            }
        }
        else
        {
            /* Ne pas ralentir une ligne en cours */
            osDelay(1);
        }
    }
}



/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM8_Init();
  MX_TSC_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_SPI2_Init();
  MX_TIM2_Init();
  MX_CAN1_Init();
  MX_LPUART1_UART_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */

  displayQueueHandle = osMessageQueueNew(
      2,
      sizeof(DisplayMessage_t),
      NULL
  );

  if (displayQueueHandle == NULL)
  {
      Error_Handler();
  }


  commandQueueHandle = osMessageQueueNew(
      8,
      sizeof(AppCommand_t),
      NULL
  );

  if (commandQueueHandle == NULL)
  {
      Error_Handler();
  }


  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of displayTask */
  displayTaskHandle = osThreadNew(StartDisplayTask, NULL, &displayTask_attributes);

  /* creation of commandTask */
  commandTaskHandle = osThreadNew(StartCommandTask, NULL, &commandTask_attributes);

  /* creation of bluetoothTask */
  bluetoothTaskHandle = osThreadNew(StartBluetoothTask, NULL, &bluetoothTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  Partage_Init();

  /* ---  : verification des taches creees par CubeMX --- */
  if (defaultTaskHandle == NULL ||
      displayTaskHandle == NULL ||
      commandTaskHandle == NULL ||
      bluetoothTaskHandle == NULL)
  {
      Error_Handler();
  }

  /* ---  : taches des capteurs et actionneurs --- */
  Son_Init();
  Boutons_Init();
  Touches_Init();
  Eclairage_Init();
  Capteurs_Init();
  Ventilation_Init();
  ReceptionCAN_Init();
  ModeJourNuit_Init();
  Gestes_Init();
  Sommeil_Init();
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_ENABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 2;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_640CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_11;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 80;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_6TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_3TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = DISABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00F12981;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief LPUART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_LPUART1_UART_Init(void)
{

  /* USER CODE BEGIN LPUART1_Init 0 */

  /* USER CODE END LPUART1_Init 0 */

  /* USER CODE BEGIN LPUART1_Init 1 */

  /* USER CODE END LPUART1_Init 1 */
  hlpuart1.Instance = LPUART1;
  hlpuart1.Init.BaudRate = 9600;
  hlpuart1.Init.WordLength = UART_WORDLENGTH_8B;
  hlpuart1.Init.StopBits = UART_STOPBITS_1;
  hlpuart1.Init.Parity = UART_PARITY_NONE;
  hlpuart1.Init.Mode = UART_MODE_TX_RX;
  hlpuart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  hlpuart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  hlpuart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&hlpuart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN LPUART1_Init 2 */

  /* USER CODE END LPUART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_2EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 79999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 99;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

}

/**
  * @brief TIM8 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM8_Init(void)
{

  /* USER CODE BEGIN TIM8_Init 0 */

  /* USER CODE END TIM8_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM8_Init 1 */

  /* USER CODE END TIM8_Init 1 */
  htim8.Instance = TIM8;
  htim8.Init.Prescaler = 80-1;
  htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim8.Init.Period = 999;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_PWM_Init(&htim8) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM8_Init 2 */

  /* USER CODE END TIM8_Init 2 */
  HAL_TIM_MspPostInit(&htim8);

}

/**
  * @brief TSC Initialization Function
  * @param None
  * @retval None
  */
static void MX_TSC_Init(void)
{

  /* USER CODE BEGIN TSC_Init 0 */

  /* USER CODE END TSC_Init 0 */

  /* USER CODE BEGIN TSC_Init 1 */

  /* USER CODE END TSC_Init 1 */

  /** Configure the TSC peripheral
  */
  htsc.Instance = TSC;
  htsc.Init.CTPulseHighLength = TSC_CTPH_2CYCLES;
  htsc.Init.CTPulseLowLength = TSC_CTPL_2CYCLES;
  htsc.Init.SpreadSpectrum = DISABLE;
  htsc.Init.SpreadSpectrumDeviation = 1;
  htsc.Init.SpreadSpectrumPrescaler = TSC_SS_PRESC_DIV1;
  htsc.Init.PulseGeneratorPrescaler = TSC_PG_PRESC_DIV4;
  htsc.Init.MaxCountValue = TSC_MCV_8191;
  htsc.Init.IODefaultMode = TSC_IODEF_OUT_PP_LOW;
  htsc.Init.SynchroPinPolarity = TSC_SYNC_POLARITY_FALLING;
  htsc.Init.AcquisitionMode = TSC_ACQ_MODE_NORMAL;
  htsc.Init.MaxCountInterrupt = DISABLE;
  htsc.Init.ChannelIOs = TSC_GROUP1_IO1|TSC_GROUP1_IO2|TSC_GROUP1_IO3;
  htsc.Init.ShieldIOs = 0;
  htsc.Init.SamplingIOs = TSC_GROUP1_IO4;
  if (HAL_TSC_Init(&htsc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TSC_Init 2 */

  /* USER CODE END TSC_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel3_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOH, SHUT1_Pin|SHUT2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, DIS_Pin|TC72_CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(MTR_CSN_GPIO_Port, MTR_CSN_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SHUT1_Pin SHUT2_Pin */
  GPIO_InitStruct.Pin = SHUT1_Pin|SHUT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);

  /*Configure GPIO pin : SW2_Pin */
  GPIO_InitStruct.Pin = SW2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SW2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : DIR_Pin */
  GPIO_InitStruct.Pin = DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DIR_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SW1_Pin */
  GPIO_InitStruct.Pin = SW1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(SW1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : INTOF2_Pin INTOF1_Pin */
  GPIO_InitStruct.Pin = INTOF2_Pin|INTOF1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : DIS_Pin TC72_CE_Pin MTR_CSN_Pin */
  GPIO_InitStruct.Pin = DIS_Pin|TC72_CE_Pin|MTR_CSN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */

    AppClock_Time_t currentTime;
    AppAlarm_t alarm;

    uint8_t lastSecond = 255;
    uint8_t lastEnabled = 255;
    uint8_t lastAlarmHour = 255;
    uint8_t lastAlarmMinute = 255;

    AppAlarm_State_t lastState =
        (AppAlarm_State_t)255;
    /* Y1.3 : suivi du son */
    AppAlarm_State_t dernierEtatSon = APP_ALARM_SILENCIEUSE;
    uint8_t bipStopEnAttente = 0;


    /* Initialisation RTC */
    if (AppClock_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    if (AppClock_Read(&hrtc, &currentTime) != HAL_OK)
    {
        Error_Handler();
    }


    /* Charger la configuration sauvegardee */
    if (!AppAlarmStorage_Load(
            &hrtc,
            &alarm,
            10))
    {
        /*
         * Premier demarrage :
         * alarme a 07:00, desactivee.
         */
        AppAlarm_Init(
            &alarm,
            7,
            0,
            10
        );

        AppAlarm_Stop(&alarm);

        /* Sauvegarder les valeurs initiales */
        if (AppAlarmStorage_Save(
                &hrtc, &alarm) != HAL_OK)
        {
            Error_Handler();
        }
    }

    /* Variables conservees pour la commande REARM */
    uint8_t alarmHour = alarm.hours;
    uint8_t alarmMinute = alarm.minutes;

    /*
     * Eviter une sonnerie immediate apres RESET
     * si on se trouve deja dans la minute
     * programmee.
     */
    if (currentTime.hours == alarm.hours &&
        currentTime.minutes == alarm.minutes)
    {
        alarm.minuteArmed = 0;
    }


    for (;;)
    {
        if (AppClock_Read(
                &hrtc,
                &currentTime) == HAL_OK)
        {
            /* Actualiser la machine a etats */
            AppAlarm_Update(
                &alarm,
                currentTime.hours,
                currentTime.minutes,
                currentTime.seconds
            );

            /* Lire toutes les commandes en attente */
            AppCommand_t cmd;

            while (osMessageQueueGet(
                       commandQueueHandle,
                       &cmd,
                       NULL,
                       0) == osOK)
            {
                switch (cmd.type)
                {
                    case APP_CMD_SNOOZE:

                        AppAlarm_Snooze(
                            &alarm,
                            currentTime.hours,
                            currentTime.minutes,
                            currentTime.seconds
                        );

                        break;


                    case APP_CMD_STOP:

                        /* Bip uniquement si on arrete une alarme en cours */
                        if (alarm.state == APP_ALARM_SONNERIE ||
                            alarm.state == APP_ALARM_SNOOZE)
                        {
                            bipStopEnAttente = 1;
                        }

                        AppAlarm_Stop(&alarm);

                        if (AppAlarmStorage_Save(
                                &hrtc, &alarm) != HAL_OK)
                        {
                            Error_Handler();
                        }

                        break;


                    case APP_CMD_REARM:

                        /* Programmer la minute suivante */
                        alarmHour = currentTime.hours;
                        alarmMinute =
                            currentTime.minutes + 1;

                        if (alarmMinute >= 60)
                        {
                            alarmMinute = 0;
                            alarmHour =
                                (alarmHour + 1) % 24;
                        }

                        AppAlarm_Init(
                            &alarm,
                            alarmHour,
                            alarmMinute,
                            10
                        );
                        if (AppAlarmStorage_Save(
                                    &hrtc, &alarm) != HAL_OK)
                            {
                                Error_Handler();
                            }



                        break;

                    case APP_CMD_SET_TIME:

                        if (AppClock_SetTime(
                                &hrtc,
                                cmd.hours,
                                cmd.minutes,
                                cmd.seconds) == HAL_OK)
                        {
                            /* Relire immediatement la nouvelle heure */
                            if (AppClock_Read(
                                    &hrtc, &currentTime) == HAL_OK)
                            {
                                /* Forcer le prochain affichage */
                                lastSecond = 255;
                            }
                        }

                        break;

                    case APP_CMD_SET_ALARM:
                    {
                        /* Conserver l'etat d'activation */
                        uint8_t wasEnabled = alarm.enabled;

                        /* Configurer la nouvelle heure */
                        AppAlarm_Init(
                            &alarm,
                            cmd.hours,
                            cmd.minutes,
                            10
                        );

                        /* Eviter un declenchement immediat
                           si on programme la minute actuelle */
                        if (currentTime.hours == alarm.hours &&
                            currentTime.minutes == alarm.minutes)
                        {
                            alarm.minuteArmed = 0;
                        }

                        /* Respecter une alarme deja desactivee */
                        if (!wasEnabled)
                        {
                            AppAlarm_Stop(&alarm);
                        }
                        if (AppAlarmStorage_Save(
                                    &hrtc, &alarm) != HAL_OK)
                            {
                                Error_Handler();
                            }



                        break;
                    }

                    case APP_CMD_SET_ENABLED:
                    {
                        if (cmd.enabled == 0)
                        {
                            /* Desactiver et arreter l'alarme */
                            AppAlarm_Stop(&alarm);
                        }
                        else
                        {
                            /* Reactiver avec l'heure programmee */
                            AppAlarm_Init(
                                &alarm,
                                alarm.hours,
                                alarm.minutes,
                                10
                            );

                            /* Ne pas declencher immediatement
                               pendant la minute en cours */
                            if (currentTime.hours == alarm.hours &&
                                currentTime.minutes == alarm.minutes)
                            {
                                alarm.minuteArmed = 0;
                            }
                        }
                        if (AppAlarmStorage_Save(
                                    &hrtc, &alarm) != HAL_OK)
                            {
                                Error_Handler();
                            }



                        break;
                    }



                    default:
                        break;
                }
            }

            /* ===== Y1.2 : Reception des gestes ===== */
            Geste_t geste;

            while (osMessageQueueGet(
                       Evt_GestesHandle,
                       &geste,
                       NULL,
                       0) == osOK)
            {
                switch (geste)
                {
                    case GESTE_SNOOZE:
                        /* Snooze seulement pendant SONNERIE */
                        AppAlarm_Snooze(
                            &alarm,
                            currentTime.hours,
                            currentTime.minutes,
                            currentTime.seconds
                        );
                        break;

                    case GESTE_STOP:
                        /* STOP autorise en SONNERIE et SNOOZE */
                        if (alarm.state == APP_ALARM_SONNERIE ||
                            alarm.state == APP_ALARM_SNOOZE)
                        {
                        	bipStopEnAttente = 1;
                            AppAlarm_Stop(&alarm);

                            /* Sauvegarder l'alarme desactivee */
                            if (AppAlarmStorage_Save(
                                    &hrtc,
                                    &alarm) != HAL_OK)
                            {
                                Error_Handler();
                            }
                        }
                        break;

                    default:
                        break;
                }
            }


            /* ===== Y1.3 : commande de la tache Son ===== */

            /* Detecter les changements d'etat de l'alarme */
            if (alarm.state != dernierEtatSon)
            {
                OrdreSon_t ordreSon;

                if (alarm.state == APP_ALARM_SONNERIE)
                {
                    /* L'alarme commence ou reprend */
                    ordreSon = MELODIE_ON;
                }
                else
                {
                    /* SNOOZE, STOP ou desactivation */
                    ordreSon = MELODIE_OFF;
                }

                /* Envoyer l'ordre a la tache Son */
                if (osMessageQueuePut(
                        Ordres_SonHandle,
                        &ordreSon,
                        0,
                        0) == osOK)
                {
                    dernierEtatSon = alarm.state;
                }
            }

            /* Bip de confirmation apres un STOP */
            if (bipStopEnAttente &&
                alarm.state == APP_ALARM_SILENCIEUSE &&
                dernierEtatSon == APP_ALARM_SILENCIEUSE)
            {
                OrdreSon_t ordreSon = BIP;

                if (osMessageQueuePut(
                        Ordres_SonHandle,
                        &ordreSon,
                        0,
                        0) == osOK)
                {
                    bipStopEnAttente = 0;
                }
            }




            /* Partager l'etat du reveil avec les autres taches */
            EtatReveil_t etatPartage = {
                .h = currentTime.hours,
                .m = currentTime.minutes,
                .s = currentTime.seconds,
                .alarme_h = alarm.hours,
                .alarme_m = alarm.minutes,
                .alarme_active = alarm.enabled,
                .etat = alarm.state
            };

            EtatReveil_Ecrire(&etatPartage);


            /* Envoyer l'etat a la tache OLED */

            if (currentTime.seconds != lastSecond ||
                alarm.state != lastState ||
                alarm.enabled != lastEnabled ||
                alarm.hours != lastAlarmHour ||
                alarm.minutes != lastAlarmMinute)

            {
                DisplayMessage_t msg;

                msg.hours = currentTime.hours;
                msg.minutes = currentTime.minutes;
                msg.seconds = currentTime.seconds;
                msg.state = alarm.state;
                msg.enabled = alarm.enabled;
                msg.alarmHours = alarm.hours;
                msg.alarmMinutes = alarm.minutes;

                /* Communication sans attente */
                if (osMessageQueuePut(
                        displayQueueHandle,
                        &msg,
                        0,
                        0) != osOK)
                {
                    DisplayMessage_t discarded;

                    (void)osMessageQueueGet(
                        displayQueueHandle,
                        &discarded,
                        NULL,
                        0
                    );

                    (void)osMessageQueuePut(
                        displayQueueHandle,
                        &msg,
                        0,
                        0
                    );
                }

                lastSecond = currentTime.seconds;
                lastState = alarm.state;
                lastEnabled = alarm.enabled;
                lastAlarmHour = alarm.hours;
                lastAlarmMinute = alarm.minutes;
            }
        }

        osDelay(100);
    }

  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartDisplayTask */
/**
* @brief Function implementing the displayTask thread.
* @param argument: Not used
* @retval None
*/

/* USER CODE END Header_StartDisplayTask */
void StartDisplayTask(void *argument)
{
  /* USER CODE BEGIN StartDisplayTask */
	  DisplayMessage_t msg;

	  /* Une seule tache initialise et utilise l'OLED (bus I2C partage : mutex) */
	  osMutexAcquire(I2C1_MutexHandle, osWaitForever);
	  OLED_Init();
	  osMutexRelease(I2C1_MutexHandle);

	  for (;;)
	  {
	    /* Attendre les nouvelles donnees */
	    if (osMessageQueueGet(displayQueueHandle, &msg, NULL, osWaitForever) == osOK)
	    {
	      osMutexAcquire(I2C1_MutexHandle, osWaitForever);
	      OLED_ShowAlarm(msg.hours, msg.minutes, msg.seconds,
	                     msg.alarmHours, msg.alarmMinutes,
	                     msg.state, msg.enabled);
	      osMutexRelease(I2C1_MutexHandle);
	    }
	  }
  /* USER CODE END StartDisplayTask */
}

/* USER CODE BEGIN Header_StartCommandTask */
/**
* @brief Function implementing the commandTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartCommandTask */
void StartCommandTask(void *argument)
{
  /* USER CODE BEGIN StartCommandTask */
	  (void)argument;
	  App_RunTerminal(&huart2, 0);
  /* USER CODE END StartCommandTask */
}

/* USER CODE BEGIN Header_StartBluetoothTask */
/**
* @brief Function implementing the bluetoothTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartBluetoothTask */
void StartBluetoothTask(void *argument)
{
  /* USER CODE BEGIN StartBluetoothTask */
	  (void)argument;
	  App_RunTerminal(&hlpuart1, 1);
  /* USER CODE END StartBluetoothTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
