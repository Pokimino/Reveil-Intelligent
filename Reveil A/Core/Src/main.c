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

#include "oled.h"
#include "app_clock.h"
#include "app_alarm.h"

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
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

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
/* USER CODE BEGIN PV */
volatile uint8_t oled_detected = 0;
volatile uint8_t oled_address = 0;
static osMessageQueueId_t displayQueueHandle;
static osMessageQueueId_t commandQueueHandle;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
void StartDefaultTask(void *argument);
void StartDisplayTask(void *argument);
void StartCommandTask(void *argument);

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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
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

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  if (defaultTaskHandle == NULL ||
      displayTaskHandle == NULL)
  {
      Error_Handler();
  }
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
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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

    /* Initialisation RTC */
    if (AppClock_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    if (AppClock_Read(&hrtc, &currentTime) != HAL_OK)
    {
        Error_Handler();
    }

    /* Premiere alarme : minute suivante */
    uint8_t alarmHour = currentTime.hours;
    uint8_t alarmMinute = currentTime.minutes + 1;

    if (alarmMinute >= 60)
    {
        alarmMinute = 0;
        alarmHour = (alarmHour + 1) % 24;
    }

    /* SNOOZE de 10 secondes pour les tests */
    AppAlarm_Init(
        &alarm,
        alarmHour,
        alarmMinute,
        10
    );

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

                        AppAlarm_Stop(&alarm);
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

                        break;
                    }



                    default:
                        break;
                }
            }

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

	/* Une seule tache initialise et utilise l'OLED */
	OLED_Init();

  /* Infinite loop */
  for(;;)
  {
	  /* Attendre les nouvelles donnees */
	      if (osMessageQueueGet(
	              displayQueueHandle,
	              &msg,
	              NULL,
	              osWaitForever) == osOK)
	      {
	    	  OLED_ShowAlarm(
	    	      msg.hours,
	    	      msg.minutes,
	    	      msg.seconds,
	    	      msg.alarmHours,
	    	      msg.alarmMinutes,
	    	      msg.state,
	    	      msg.enabled
	    	  );
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
  /* Infinite loop */


	uint8_t rx;

	char line[16];
	uint8_t len = 0;

	const char welcome[] =
	    "\r\nREVEIL INTELLIGENT\r\n"
	    "T HH MM SS = regler heure\r\n"
	    "s = SNOOZE\r\n"
	    "x = STOP\r\n"
	    "r = REARM\r\n"
			"A HH MM = regler alarme\r\n"
			"E 0 / E 1 = OFF / ON\r\n"
			;

	HAL_UART_Transmit(
	    &huart2,
	    (uint8_t *)welcome,
	    sizeof(welcome) - 1,
	    100
	);

	for (;;)
	{
	    if (HAL_UART_Receive(
	            &huart2,
	            &rx,
	            1,
	            1) == HAL_OK)
	    {
	        AppCommand_t cmd = {0};
	        uint8_t send = 0;

	        /* Commandes simples immediates */
	        if (len == 0 &&
	            (rx == 's' || rx == 'S' ||
	             rx == 'x' || rx == 'X' ||
	             rx == 'r' || rx == 'R'))
	        {
	            if (rx == 's' || rx == 'S')
	                cmd.type = APP_CMD_SNOOZE;

	            else if (rx == 'x' || rx == 'X')
	                cmd.type = APP_CMD_STOP;

	            else
	                cmd.type = APP_CMD_REARM;

	            send = 1;
	        }


	        /* Fin d'une commande complete */
	        else if (rx == '\r' || rx == '\n')
	        {
	            if (len > 0)
	            {
	                uint8_t h = 0;
	                uint8_t m = 0;
	                uint8_t s = 0;

	                /* T HH MM SS : regler le RTC */
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
	                    cmd.type = APP_CMD_SET_TIME;
	                    cmd.hours = h;
	                    cmd.minutes = m;
	                    cmd.seconds = s;
	                    send = 1;
	                }

	                /* A HH MM : programmer l'alarme */
	                else if (len == 7 &&
	                    (line[0] == 'A' || line[0] == 'a') &&
	                    line[1] == ' ' &&
	                    line[4] == ' ' &&
	                    App_Parse2Digits(&line[2], &h) &&
	                    App_Parse2Digits(&line[5], &m) &&
	                    h < 24 && m < 60)
	                {
	                    cmd.type = APP_CMD_SET_ALARM;
	                    cmd.hours = h;
	                    cmd.minutes = m;
	                    send = 1;
	                }

	                /* E 0 ou E 1 : activer / desactiver */
	                else if (len == 3 &&
	                    (line[0] == 'E' || line[0] == 'e') &&
	                    line[1] == ' ' &&
	                    (line[2] == '0' || line[2] == '1'))
	                {
	                    cmd.type = APP_CMD_SET_ENABLED;
	                    cmd.enabled = line[2] - '0';
	                    send = 1;
	                }

	                else
	                {
	                    const char error[] =
	                        "ERREUR: commande invalide\r\n";

	                    HAL_UART_Transmit(
	                        &huart2,
	                        (uint8_t *)error,
	                        sizeof(error) - 1,
	                        100
	                    );
	                }

	                len = 0;
	            }
	        }


	        /* Retour arriere */
	        else if (rx == '\b' || rx == 127)
	        {
	            if (len > 0)
	                len--;
	        }

	        /* Accumulation de la ligne */
	        else if (rx >= 32 && rx <= 126)
	        {
	            if (len < sizeof(line) - 1)
	            {
	                line[len++] = (char)rx;
	            }
	            else
	            {
	                len = 0;

	                const char error[] =
	                    "ERREUR: ligne trop longue\r\n";

	                HAL_UART_Transmit(
	                    &huart2,
	                    (uint8_t *)error,
	                    sizeof(error) - 1,
	                    100
	                );
	            }
	        }

	        /* Envoyer la commande */
	        if (send)
	        {
	            if (osMessageQueuePut(
	                    commandQueueHandle,
	                    &cmd,
	                    0,
	                    0) == osOK)
	            {
	                const char ack[] =
	                    "Commande envoyee\r\n";

	                HAL_UART_Transmit(
	                    &huart2,
	                    (uint8_t *)ack,
	                    sizeof(ack) - 1,
	                    100
	                );
	            }
	            else
	            {
	                const char error[] =
	                    "ERREUR: queue pleine\r\n";

	                HAL_UART_Transmit(
	                    &huart2,
	                    (uint8_t *)error,
	                    sizeof(error) - 1,
	                    100
	                );
	            }
	        }
	    }

	    osDelay(10);
	}


  /* USER CODE END StartCommandTask */
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
#ifdef USE_FULL_ASSERT
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
