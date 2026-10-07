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
#include <stdio.h>
#include <string.h>
#include "ssd1306.h"
#include "SGP40.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define NB_LED				4
#define POT_SEUIL_MANUEL	50
#define DUREE_AUBE_MS       15000U
#define DUREE_CREPUSCULE_MS 15000U
#define DUREE_ALARME_MS     30000U
#define TOUCH_THRESHOLD 430
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef hlpuart1;
UART_HandleTypeDef huart2;

SPI_HandleTypeDef hspi2;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim8;
DMA_HandleTypeDef hdma_tim3_ch4_up;

TSC_HandleTypeDef htsc;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */
/* ============================================================
 * HORLOGE ET ALARME
 * ============================================================ */

typedef struct
{
    uint8_t heure;
    uint8_t min;
    uint8_t sec;
} temps;

temps horloge = {0, 0, 0};
temps alarme  = {0, 0, 0};

uint8_t alarmeActive = 0;


/* ============================================================
 * AFFICHAGE OLED
 * ============================================================ */

char Oled[20];

/*
 * 0 = Heure
 * 1 = Alarme
 * 2 = Température
 * 3 = Qualité de l'air
 */
uint8_t ecran = 0;

volatile uint8_t oledColor = 1;


/* ============================================================
 * BLUETOOTH
 * ============================================================ */

uint8_t bt_char;
char bt_buffer[30];
uint8_t bt_index = 0;


/* ============================================================
 * SEUILS DE CONFIGURATION
 * ============================================================ */

volatile int16_t  seuilTemperature = 3000;   // 30.00 °C
volatile uint16_t seuilVOC = 0;
volatile uint16_t seuilLDR = 1500;


/* ============================================================
 * CAPTEUR DE TEMPÉRATURE
 * ============================================================ */

uint8_t control_T[2] = {0x80, 0x15};
uint8_t Temp_Address = 0x02;
uint8_t Temp[3];

int16_t temperature = 0;
uint8_t temperatureValide = 0;


/* ============================================================
 * CAPTEUR VOC
 * ============================================================ */

uint16_t voc = 0;

float voc_temp = 25.0;
float voc_humi = 50.0;

uint8_t vocValide = 0;


/* ============================================================
 * ADC : POTENTIOMÈTRE ET LDR
 * ============================================================ */

volatile uint16_t adcPot = 0;
volatile uint16_t adcLdr = 0;


/* ============================================================
 * LED RGB
 * ============================================================ */

uint16_t PwmLED[NB_LED * 24 + 50];


/* ============================================================
 * SIMULATION D'AUBE ET DE CRÉPUSCULE
 * ============================================================ */

volatile uint8_t aubeActive = 0;
volatile uint8_t crepusculeActif = 0;

TickType_t debutAube = 0;
TickType_t debutCrepuscule = 0;


/* ============================================================
 * BUZZER ET MÉLODIE DE SOMMEIL
 * ============================================================ */

typedef struct
{
    uint16_t frequence;
    uint16_t duree_ms;
    uint16_t pause_ms;
} NoteDouce;

const NoteDouce melodieDouce[] =
{
    {392, 500, 150},   // Sol
    {440, 500, 150},   // La
    {392, 700, 250},   // Sol

    {330, 500, 150},   // Mi
    {349, 500, 150},   // Fa
    {330, 700, 300},   // Mi

    {294, 600, 150},   // Ré
    {330, 600, 150},   // Mi
    {262, 900, 500}    // Do
};

volatile uint8_t melodieSommeilDemandee = 0;


/* ============================================================
 * Sensitive TouchPad
 * ============================================================ */

volatile uint16_t touchStop   = 0;
volatile uint16_t touchSnooze = 0;
volatile uint16_t touchOnOff  = 0;

volatile uint8_t demandeStopAlarme = 0;

volatile uint8_t demandeSnooze = 0;
volatile uint8_t snoozeActif = 0;
temps heureSnooze = {0, 0, 0};


/* ============================================================
 * SYNCHRONISATION FREERTOS
 * ============================================================ */

SemaphoreHandle_t semaphBT;
SemaphoreHandle_t semaphSW2;
SemaphoreHandle_t semaphSW1;

SemaphoreHandle_t mutexI2C;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_LPUART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI2_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_ADC1_Init(void);
static void MX_TSC_Init(void);
void StartDefaultTask(void const * argument);

/* USER CODE BEGIN PFP */
void TaskHorloge(void *pvParameters);
void TaskAlarme(void *pvParameters);
void playAlarme(void);

void TaskAffichage(void *pvParameters);

void TaskSW1(void *pvParameters);
void TaskSW2(void *pvParameters);

void TaskBluetooth(void *pvParameters);

void TaskTemperature(void *pvParameters);

void TaskVOC(void *pvParameters);

void TaskADC(void *pvParameters);

void TaskVentilo(void *pvParameters);
void Ventilo_Start(uint8_t direction, uint32_t vitesse);
void Ventilo_Stop(void);

void TaskLED(void *pvParameters);
void LED_SetColor(uint8_t red, uint8_t green, uint8_t blue);
void LED_Send(void);

void TaskMelodieSommeil(void *pvParameters);
void Buzzer_SetFrequency(uint16_t frequence);
void playMelodyDouce(void);

void TaskTouch(void *pvParameters);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Ventilo_Start(uint8_t direction, uint32_t vitesse)
{
    if (vitesse > 80000)
    {
        vitesse = 80000;
    }

    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);

    HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET);

    HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, direction ? GPIO_PIN_SET : GPIO_PIN_RESET);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, vitesse);

    HAL_GPIO_WritePin( DIS_GPIO_Port, DIS_Pin, GPIO_PIN_RESET);

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
}

void Ventilo_Stop(void)
{
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);

    HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET);
}

void LED_SetColor(uint8_t red, uint8_t green, uint8_t blue)
{
    uint8_t Couleur[12] =
    {
        green, red, blue,

        green, red, blue,

        green, red, blue,

        green, red, blue
    };

    uint16_t k = 0;

    for (int i = 0; i < 12; i++)
    {
        for (int j = 7; j >= 0; j--)
        {
            PwmLED[k++] = (Couleur[i] & (1u << j)) ? 64 : 32;
        }
    }

    for (int i = 0; i < 50; i++)
    {
        PwmLED[k++] = 0;
    }
}

void LED_Send(void)
{

    HAL_TIM_PWM_Stop_DMA(&htim3, TIM_CHANNEL_4);

    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t *)PwmLED, NB_LED * 24 + 50);

    vTaskDelay(pdMS_TO_TICKS(5));
}

void Buzzer_SetFrequency(uint16_t frequence)
{
    if (frequence == 0)
    {
        HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
        return;
    }

    uint32_t periode = 1000000U / frequence;

    __HAL_TIM_SET_AUTORELOAD(&htim8, periode - 1);
    __HAL_TIM_SET_COMPARE(&htim8,
                          TIM_CHANNEL_4,
                          periode / 2);

    __HAL_TIM_SET_COUNTER(&htim8, 0);
}

void playAlarme(void)
{
    Buzzer_SetFrequency(1000);
    HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_4);
}

void playMelodyDouce(void)
{
    uint8_t taille =
        sizeof(melodieDouce) / sizeof(NoteDouce);

    for (uint8_t i = 0; i < taille; i++)
    {
        Buzzer_SetFrequency(
            melodieDouce[i].frequence);

        HAL_TIM_PWM_Start(
            &htim8,
            TIM_CHANNEL_4);

        vTaskDelay(
            pdMS_TO_TICKS(
                melodieDouce[i].duree_ms));

        HAL_TIM_PWM_Stop(
            &htim8,
            TIM_CHANNEL_4);

        vTaskDelay(
            pdMS_TO_TICKS(
                melodieDouce[i].pause_ms));
    }

    HAL_TIM_PWM_Stop(
        &htim8,
        TIM_CHANNEL_4);
}

uint16_t TSC_ReadChannel(uint32_t channel)
{
    TSC_IOConfigTypeDef ioConfig = {0};

    ioConfig.ChannelIOs = channel;
    ioConfig.ShieldIOs = 0;
    ioConfig.SamplingIOs = TSC_GROUP1_IO4;

    HAL_TSC_IODischarge(&htsc, ENABLE);
    vTaskDelay(pdMS_TO_TICKS(1));
    HAL_TSC_IODischarge(&htsc, DISABLE);

    if (HAL_TSC_IOConfig(&htsc, &ioConfig) != HAL_OK)
        return 8191;

    if (HAL_TSC_Start(&htsc) != HAL_OK)
        return 8191;

    while (HAL_TSC_GetState(&htsc) == HAL_TSC_STATE_BUSY)
    {
        taskYIELD();
    }

    if (HAL_TSC_GetState(&htsc) != HAL_TSC_STATE_READY)
        return 8191;

    return HAL_TSC_GroupGetValue(&htsc, TSC_GROUP1_IDX);
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
  MX_I2C1_Init();
  MX_LPUART1_UART_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_TIM8_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_TSC_Init();
  /* USER CODE BEGIN 2 */

  SSD1306_Init();

  __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 0);
  HAL_GPIO_WritePin(DIS_GPIO_Port, DIS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(DIR_GPIO_Port, DIR_Pin, GPIO_PIN_RESET);

  HAL_UART_Receive_IT(&hlpuart1, &bt_char, 1);

  /* USER CODE END 2 */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */

  mutexI2C = xSemaphoreCreateMutex();

  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */

  semaphBT = xSemaphoreCreateBinary();
  semaphSW2 = xSemaphoreCreateBinary();
  semaphSW1 = xSemaphoreCreateBinary();


  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of defaultTask */
  osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
  defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */

  xTaskCreate(TaskHorloge,
              "Horloge",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskAlarme,
              "Alarme",
              256,
              NULL,
              tskIDLE_PRIORITY + 2,
              NULL);

  xTaskCreate(TaskAffichage,
              "Affichage",
              512,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskSW1,
              "SW1",
              256,
              NULL,
              tskIDLE_PRIORITY + 2,
              NULL);

  xTaskCreate(TaskSW2,
              "SW2",
              256,
              NULL,
              tskIDLE_PRIORITY + 2,
              NULL);

  xTaskCreate(TaskBluetooth,
              "Bluetooth",
              512,
              NULL,
              tskIDLE_PRIORITY + 2,
              NULL);

  xTaskCreate(TaskTemperature,
              "Temperature",
              512,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskVOC,
              "VOC",
              512,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskADC,
              "ADC",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskVentilo,
              "Ventilo",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskLED,
              "LED",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskMelodieSommeil,
              "MelodieSommeil",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  xTaskCreate(TaskTouch,
              "Touch",
              256,
              NULL,
              tskIDLE_PRIORITY + 1,
              NULL);

  /* USER CODE END RTOS_THREADS */

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
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
  sConfig.Channel = ADC_CHANNEL_11;
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
  sConfig.Channel = ADC_CHANNEL_13;
  sConfig.Rank = ADC_REGULAR_RANK_2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  hi2c1.Init.Timing = 0x00702991;
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
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;
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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 1-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 80000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 1-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 100-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
  htim8.Init.Period = 500-1;
  htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim8.Init.RepetitionCounter = 0;
  htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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
  sConfigOC.Pulse = 250;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LED2_Pin|DIR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED5_Pin|DIS_Pin|CE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LED2_Pin DIR_Pin */
  GPIO_InitStruct.Pin = LED2_Pin|DIR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : LED5_Pin DIS_Pin CE_Pin */
  GPIO_InitStruct.Pin = LED5_Pin|DIS_Pin|CE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SW2_Pin */
  GPIO_InitStruct.Pin = SW2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : SW1_Pin */
  GPIO_InitStruct.Pin = SW1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW1_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void TaskHorloge(void *pvParameters)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(1000));
        horloge.sec++;

        if (horloge.sec >= 60)
        {
            horloge.sec = 0;
            horloge.min++;

            if (horloge.min >= 60)
            {
                horloge.min = 0;
                horloge.heure++;

                if (horloge.heure >= 24)
                {
                    horloge.heure = 0;
                }
            }
        }

    }
}

void TaskAlarme(void *pvParameters)
{
    uint8_t declenchee = 0;
    TickType_t debutSonnerie = 0;
    uint8_t sonnerieActive = 0;

    while (1)
    {
        if (demandeStopAlarme)
        {
            demandeStopAlarme = 0;

            if (sonnerieActive)
            {
                HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
                sonnerieActive = 0;
            }

            aubeActive = 0;

            snoozeActif = 0;
        }

        if (demandeSnooze)
        {
            demandeSnooze = 0;

            if (sonnerieActive)
            {
                HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
                sonnerieActive = 0;

                aubeActive = 0;

                heureSnooze.heure = horloge.heure;
                heureSnooze.min   = horloge.min + 1;
                heureSnooze.sec   = horloge.sec;

                if (heureSnooze.min >= 60)
                {
                    heureSnooze.min = 0;
                    heureSnooze.heure++;

                    if (heureSnooze.heure >= 24)
                    {
                        heureSnooze.heure = 0;
                    }
                }

                snoozeActif = 1;
            }
        }

        if (alarmeActive && !snoozeActif && horloge.heure == alarme.heure && horloge.min   == alarme.min && horloge.sec   == alarme.sec && !declenchee)
        {
            declenchee = 1;

            aubeActive = 1;
            debutAube = xTaskGetTickCount();

            playAlarme();

            sonnerieActive = 1;
            debutSonnerie = xTaskGetTickCount();
        }

        if (alarmeActive && snoozeActif && horloge.heure == heureSnooze.heure && horloge.min   == heureSnooze.min && horloge.sec   == heureSnooze.sec)
        {
            snoozeActif = 0;

            aubeActive = 1;
            debutAube = xTaskGetTickCount();

            playAlarme();

            sonnerieActive = 1;
            debutSonnerie = xTaskGetTickCount();
        }

        if (sonnerieActive && (xTaskGetTickCount() - debutSonnerie) >= pdMS_TO_TICKS(DUREE_ALARME_MS))
        {
            HAL_TIM_PWM_Stop(&htim8, TIM_CHANNEL_4);
            sonnerieActive = 0;
        }

        if (horloge.heure != alarme.heure || horloge.min != alarme.min || horloge.sec != alarme.sec)
        {
            declenchee = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void TaskAffichage(void *pvParameters)
{
    while (1)
    {
        if (xSemaphoreTake(mutexI2C, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            SSD1306_Fill(0);
            SSD1306_GotoXY(0, 0);

            if (ecran == 0)
            {
                SSD1306_Puts("HEURE", &Font_11x18, oledColor);

                sprintf(Oled, "%02d:%02d:%02d", horloge.heure, horloge.min, horloge.sec);
            }

            else if (ecran == 1)
            {
                if (alarmeActive)
                {
                    SSD1306_Puts("ALARME ON", &Font_11x18, oledColor);
                }

                else
                {
                    SSD1306_Puts("ALARME OFF", &Font_11x18, oledColor);
                }

                sprintf(Oled,
                        "%02d:%02d:%02d", alarme.heure, alarme.min, alarme.sec);
            }

            else if (ecran == 2)
            {
                SSD1306_Puts("TEMPERATURE", &Font_11x18, oledColor);

                if (temperature < 0)
                {
                    int16_t t = -temperature;

                    sprintf(Oled, "-%d.%02d C", t / 100, t % 100);
                }

                else
                {
                    sprintf(Oled, "%d.%02d C", temperature / 100, temperature % 100);
                }
            }

            else
            {
                SSD1306_Puts("QUALITE AIR", &Font_11x18, oledColor);

                sprintf(Oled, "VOC : %d", voc);
            }

            SSD1306_GotoXY(0, 30);
            SSD1306_Puts(Oled, &Font_11x18, oledColor);

            SSD1306_UpdateScreen();

            xSemaphoreGive(mutexI2C);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void TaskSW1(void *pvParameters)
{
    while (1)
    {
        xSemaphoreTake(semaphSW1, portMAX_DELAY);

        if (!crepusculeActif)
        {
            crepusculeActif = 1;
            debutCrepuscule = xTaskGetTickCount();
            melodieSommeilDemandee = 1;
        }
    }
}

void TaskSW2(void *pvParameters)
{
    while (1)
    {
        xSemaphoreTake(semaphSW2, portMAX_DELAY);
        ecran = (ecran+1) % 4;
    }
}

void TaskBluetooth(void *pvParameters)
{
    int h, m, s;
    int valeur;
    char reponse[256];

    while (1)
    {
        xSemaphoreTake(semaphBT, portMAX_DELAY);

        if (bt_char == '\r' || bt_char == '\n')
        {
            if (bt_index > 0)
            {
                bt_buffer[bt_index] = '\0';

                if (sscanf(bt_buffer, "heure : %d:%d:%d", &h, &m, &s) == 3)
                {

                    if ((h >= 0 && h < 24) && (m >= 0 && m < 60) && (s >= 0 && s < 60))
                    {
                        horloge.heure = h;
                        horloge.min   = m;
                        horloge.sec   = s;

                        sprintf(reponse, "OK heure : %02d:%02d:%02d\r\n", h, m, s);
                    }

                    else
                    {
                        sprintf(reponse, "ERREUR heure invalide\r\n");
                    }

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (sscanf(bt_buffer, "alarme : %d:%d:%d", &h, &m, &s) == 3)
                {

                    if ((h >= 0 && h < 24) && (m >= 0 && m < 60) && (s >= 0 && s < 60))
                    {
                        alarme.heure = h;
                        alarme.min   = m;
                        alarme.sec   = s;

                        alarmeActive = 1;

                        sprintf(reponse, "OK alarme : %02d:%02d:%02d\r\n", h, m, s);
                    }

                    else
                    {
                        sprintf(reponse, "ERREUR alarme invalide\r\n");
                    }

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (strcmp(bt_buffer, "alarme off") == 0)
                {

                    alarmeActive = 0;

                    sprintf(reponse, "OK alarme desactivee\r\n");

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (strcmp(bt_buffer, "alarme on") == 0)
                {
                    alarmeActive = 1;

                    sprintf(reponse, "OK alarme activee : %02d:%02d:%02d\r\n", alarme.heure, alarme.min, alarme.sec);

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (sscanf(bt_buffer, "temperature : %d", &valeur) == 1)
                {
                    if (valeur >= -40 && valeur <= 125)
                    {
                        seuilTemperature = (int16_t)(valeur * 100);

                        sprintf(reponse, "OK temperature : %d C\r\n", valeur);
                    }

                    else
                    {
                        sprintf(reponse, "ERREUR temperature invalide\r\n");
                    }

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (sscanf(bt_buffer, "voc : %d", &valeur) == 1)
                {

                    if (valeur >= 0 && valeur <= 65535)
                    {
                        seuilVOC = (uint16_t)valeur;
                        sprintf(reponse, "OK voc : %d\r\n", valeur);
                    }

                    else
                    {
                        sprintf(reponse, "ERREUR voc invalide\r\n");
                    }

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (sscanf(bt_buffer, "ldr : %d", &valeur) == 1)
                {
                    if (valeur >= 0 && valeur <= 4095)
                    {
                        seuilLDR = (uint16_t)valeur;

                        sprintf(reponse, "OK ldr : %d\r\n", valeur);
                    }

                    else
                    {
                        sprintf(reponse, "ERREUR ldr invalide\r\n");
                    }

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }


                else if (strcmp(bt_buffer, "status") == 0)
                {
                    sprintf(reponse,

                            "Heure : %02d:%02d:%02d\r\n"
                            "Alarme : %02d:%02d:%02d [%s]\r\n"
                            "Temperature seuil : %d C\r\n"
                            "VOC seuil : %u\r\n"
                            "LDR seuil : %u\r\n",

                            horloge.heure, horloge.min, horloge.sec,
                            alarme.heure, alarme.min, alarme.sec,
                            alarmeActive ? "ON" : "OFF",
                            seuilTemperature / 100,
                            seuilVOC,
                            seuilLDR);

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }

                else
                {
                    sprintf(reponse, "ERREUR commande inconnue\r\n");

                    HAL_UART_Transmit(&hlpuart1, (uint8_t *)reponse, strlen(reponse), HAL_MAX_DELAY);
                }

                bt_index = 0;
            }
        }

        else
        {
            if (bt_index < sizeof(bt_buffer) - 1)
            {
                bt_buffer[bt_index++] = bt_char;
            }

            else
            {
                bt_index = 0;
            }

        }

        HAL_UART_Receive_IT(&hlpuart1, &bt_char, 1);
    }
}

void TaskTemperature(void *pvParameters)
{
    uint8_t msbvalue;
    uint8_t lsbvalue;
    uint16_t temp;
    uint8_t sign;

    while (1)
    {
        HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);

        HAL_SPI_Transmit(&hspi2, (uint8_t *)control_T, sizeof(control_T), HAL_MAX_DELAY);

        HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

        vTaskDelay(pdMS_TO_TICKS(150));

        HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_SET);

        HAL_SPI_Transmit(&hspi2, &Temp_Address, 1, HAL_MAX_DELAY);

        HAL_SPI_Receive(&hspi2, (uint8_t *)Temp, 3, HAL_MAX_DELAY);

        HAL_GPIO_WritePin(CE_GPIO_Port, CE_Pin, GPIO_PIN_RESET);

        msbvalue = Temp[0];

        lsbvalue = Temp[1];

        temp = ((((uint16_t)(msbvalue & 0x7F) << 2) | (lsbvalue >> 6)) * 25);

        sign = (msbvalue & 0x80) ? 1 : 0;

        if (sign)
            temperature = -(int16_t)temp;

        else
            temperature = (int16_t)temp;

        temperatureValide = 1;

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskVOC(void *pvParameters)
{
    while (1)
    {
        voc_temp = temperature / 100.0f;
        voc_humi = 50.0f;

        if (xSemaphoreTake(mutexI2C, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            voc = SGP40_MeasureVOC(voc_temp, voc_humi);
            vocValide = 1;

            xSemaphoreGive(mutexI2C);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void TaskADC(void *pvParameters)
{
    while (1)
    {
        HAL_ADC_Start(&hadc1);

        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            adcPot = HAL_ADC_GetValue(&hadc1);
        }

        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK)
        {
            adcLdr = HAL_ADC_GetValue(&hadc1);
            oledColor = (adcLdr < seuilLDR) ? 1 : 0;
        }

        HAL_ADC_Stop(&hadc1);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TaskVentilo(void *pvParameters)
{
    uint8_t direction = 0;
    uint8_t modePrecedent = 0xFF;
    TickType_t dernierChangement = 0;
    const TickType_t periode = pdMS_TO_TICKS(2000);

    Ventilo_Stop();

    while (1)
    {
        TickType_t maintenant = xTaskGetTickCount();

        if (adcPot > POT_SEUIL_MANUEL)
        {
            uint32_t vitesse = ((uint32_t)adcPot * 79999U) / 4095U;
            Ventilo_Start(0, vitesse);
            modePrecedent = 3;
        }

        else
        {
        	if (temperatureValide && temperature > seuilTemperature)
            {
                if (modePrecedent != 1)
                {
                    Ventilo_Start(0, 80000);
                    modePrecedent = 1;
                }

            }

            else if (vocValide && voc < seuilVOC)
            {
                if (modePrecedent != 2)
                {
                    direction = 0;
                    Ventilo_Start(direction, 60000);
                    dernierChangement = maintenant;
                    modePrecedent = 2;
                }

                if ((maintenant - dernierChangement) >= periode)
                {
                    direction = !direction;
                    Ventilo_Start(direction, 60000);
                    dernierChangement = maintenant;
                }
            }

            else
            {
                if (modePrecedent != 0)
                {
                    Ventilo_Stop();
                    modePrecedent = 0;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void TaskLED(void *pvParameters)
{

    uint8_t red = 0;
    uint8_t green = 0;
    uint8_t blue = 0;

    TickType_t maintenant;
    uint32_t tempsEcoule;
    uint32_t progression;

    while (1)
    {
        maintenant = xTaskGetTickCount();

        if (crepusculeActif)
        {
            tempsEcoule = (uint32_t) ((maintenant - debutCrepuscule) * portTICK_PERIOD_MS);

            if (tempsEcoule < DUREE_CREPUSCULE_MS)
            {
                progression = (tempsEcoule * 255U) / DUREE_CREPUSCULE_MS;

                red = (uint8_t) (255U - progression);

                green = (uint8_t) ((100U * (255U - progression)) / 255U);

                blue = (uint8_t) ((20U * (255U - progression)) / 255U);
            }

            else
            {

                red = 0;
                green = 0;
                blue = 0;
                crepusculeActif = 0;
            }
        }


        else if (aubeActive)
        {
            tempsEcoule = (uint32_t) ((maintenant - debutAube) * portTICK_PERIOD_MS);

            if (tempsEcoule < DUREE_AUBE_MS)
            {
                progression = (tempsEcoule * 255U) / DUREE_AUBE_MS;

                red = (uint8_t)progression;

                green = (uint8_t) ((progression * 180U) / 255U);

                blue = (uint8_t) (40U + ((progression * 100U) / 255U));
            }

            else
            {
                red   = 0;
                green = 0;
                blue  = 0;
                aubeActive = 0;
            }
        }


        else
        {
            red = 0;
            green = 0;
            blue = 0;
        }

        LED_SetColor( red, green, blue);
        LED_Send();

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void TaskMelodieSommeil(void *pvParameters)
{
    while (1)
    {
        if (melodieSommeilDemandee)
        {
            melodieSommeilDemandee = 0;
            playMelodyDouce();
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (huart->Instance == LPUART1)
    {
        xSemaphoreGiveFromISR(semaphBT, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    static uint32_t dernierSW1 = 0;
    static uint32_t dernierSW2 = 0;

    uint32_t maintenant = HAL_GetTick();

    if (GPIO_Pin == SW2_Pin)
    {
        if ((maintenant - dernierSW2) >= 200U)
        {
            dernierSW2 = maintenant;
            xSemaphoreGiveFromISR(semaphSW2, &xHigherPriorityTaskWoken);
        }
    }

    if (GPIO_Pin == SW1_Pin)
    {
        if ((maintenant - dernierSW1) >= 200U)
        {
            dernierSW1 = maintenant;
            xSemaphoreGiveFromISR(semaphSW1, &xHigherPriorityTaskWoken);
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void TaskTouch(void *pvParameters)
{
    uint8_t stopPrecedent   = 0;
    uint8_t snoozePrecedent = 0;
    uint8_t onOffPrecedent  = 0;

    while (1)
    {
        touchStop = TSC_ReadChannel(TSC_GROUP1_IO1);
        touchSnooze = TSC_ReadChannel(TSC_GROUP1_IO2);
        touchOnOff = TSC_ReadChannel(TSC_GROUP1_IO3);

        uint8_t stopTouche = (touchStop < TOUCH_THRESHOLD);

        uint8_t snoozeTouche = (touchSnooze < TOUCH_THRESHOLD);

        uint8_t onOffTouche = (touchOnOff < TOUCH_THRESHOLD);

        if (stopTouche && !stopPrecedent)
        {
            demandeStopAlarme = 1;
        }

        if (snoozeTouche && !snoozePrecedent)
        {
            demandeSnooze = 1;
        }


        if (onOffTouche && !onOffPrecedent)
        {
            alarmeActive = !alarmeActive;

            if (!alarmeActive)
            {
                demandeStopAlarme = 1;
            }
        }

        stopPrecedent   = stopTouche;
        snoozePrecedent = snoozeTouche;
        onOffPrecedent  = onOffTouche;

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
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
  if (htim->Instance == TIM6) {
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
