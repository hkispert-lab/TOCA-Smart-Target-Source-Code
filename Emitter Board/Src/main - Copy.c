/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "UART.h"
#include "LEDs.h"
#include "Flash.h"
#include <string.h>
#include <stdbool.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

CRC_HandleTypeDef hcrc;

I2C_HandleTypeDef hi2c1;

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef hdma_spi1_tx;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
DMA_HandleTypeDef hdma_tim1_ch1;
DMA_HandleTypeDef hdma_tim1_ch2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

bool EmittersInitialized = false;
uint16_t Emitters_DMA_complete_cnt_ISR  = 0;
uint16_t Emitters_DMA_complete_cnt_task = 0;

// 12 emitter PWM periods per us
// 29.583 us per time slot = 355 PWM periods per time slot
// 32 time slots plus 1 off state
// terminate each slot with zero to leave emitter off
#define SLOT_SIZE 355                   // 355/12 = 29.583 us

#pragma data_alignment=4
uint16_t Emitter_Select_Codes_0_15[SLOT_SIZE*NUM_SLOTS+1];      // the plus 1 is for terminating the last slot with zero
#pragma data_alignment=4
uint16_t Emitter_Select_Codes_16_31[SLOT_SIZE*NUM_SLOTS+1];     // the plus 1 is for terminating the last slot with zero

//------------------------------------------------------------------------------
// slot configuration
// 32 slots, each slot contains (emitter number, PWM percent)
Emitter_slot_rec_t Emitter_slot_rec[NUM_SLOTS];

//------------------------------------------------------------------------------
void Init_Emitter_Select_Codes(uint16_t slot) {
  uint16_t *pEmitter_select_codes;
  uint32_t increment;
  uint32_t accumulator;
  uint16_t mask;
  uint16_t i;
  uint16_t emitter;
  uint16_t pct;

  // ignore invalid slot
  if (slot >= NUM_SLOTS) return;

  // requested emitter and pct for this slot
  emitter = Emitter_slot_rec_request[slot].emitter;
  pct     = Emitter_slot_rec_request[slot].pct;

  // ignore invalid request
  if ((emitter >= NUM_EMITTERS) ||
      (pct     >  100)) return;

  // ignore request if emitter and pct are already configured as requested
  if ((Emitter_slot_rec[slot].emitter == emitter) &&
      (Emitter_slot_rec[slot].pct     == pct)) return;

  // record the requested slot configuration
  Emitter_slot_rec[slot].emitter = emitter;
  Emitter_slot_rec[slot].pct     = pct;

  if (pct >= 100) {
    increment   = 65536ul;
    accumulator = 0;
    }
  else if (pct >= 1) {
    increment   = pct*655;
    accumulator = 65536ul - increment;
    }
  else {
    increment   = 0;
    accumulator = 0;
    }

  if (emitter < 16) {pEmitter_select_codes = &Emitter_Select_Codes_0_15 [slot*SLOT_SIZE];}
  else              {pEmitter_select_codes = &Emitter_Select_Codes_16_31[slot*SLOT_SIZE]; emitter -= 16;}
  mask = 1u << emitter;

  for (i = 0; i < SLOT_SIZE; i++) {
    accumulator += increment;
    if (accumulator >> 16) {*pEmitter_select_codes |=  mask; accumulator &= 0xfffful;}
    else                   {*pEmitter_select_codes &= ~mask;}
    pEmitter_select_codes++;
    }
}

//------------------------------------------------------------------------------
// there is a brief moment after DMA completes to update the next slot
// update the next slot after emitter DMA complete
void Emitter_PWM_task(void) {
  static uint16_t slot = 0;

  if ((int16_t) (Emitters_DMA_complete_cnt_ISR - Emitters_DMA_complete_cnt_task) > 0) {
    Emitters_DMA_complete_cnt_task++;
    Init_Emitter_Select_Codes(slot);
    if (++slot >= NUM_SLOTS) slot = 0;
    }
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_ADC1_Init(void);
static void MX_CRC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//------------------------------------------------------------------------------
void EmittersSync(void) {
  if (EmittersInitialized) {
    // disable DMA request enable to consume any pending requests
    TIM1->DIER &= ~(TIM_DIER_CC2DE | TIM_DIER_CC1DE);

    // start DMA emitter selector sequence
    HAL_DMA_Start_IT(&hdma_tim1_ch1, (uint32_t) Emitter_Select_Codes_0_15,  (uint32_t) &IR_EN0_GPIO_Port ->ODR, sizeof(Emitter_Select_Codes_0_15)/2);   // DMA 2 stream 1
    HAL_DMA_Start_IT(&hdma_tim1_ch2, (uint32_t) Emitter_Select_Codes_16_31, (uint32_t) &IR_EN16_GPIO_Port->ODR, sizeof(Emitter_Select_Codes_16_31)/2);  // DMA 2 stream 2

    // enable DMA requests
    TIM1->DIER |= (TIM_DIER_CC2DE | TIM_DIER_CC1DE);

    //TIM1->CNT  = 0;
    TIM1->CR1 |= TIM_CR1_CEN;
    }
}

//------------------------------------------------------------------------------
// test code for pins that are defined but not yet implemented
//
uint16_t Set_AUX             = 0;
uint16_t Set_DEBUG_PE12      = 0;
uint16_t Set_DEBUG_PE14      = 0;
uint16_t Set_DEBUG_PE15      = 0;
uint16_t Read_IR_PGOOD       = 0;
uint16_t Read_V3_3_PGOOD     = 0;
uint16_t Read_PWR_IN_FAULT_A = 0;
uint16_t Read_PWR_IN_FAULT_B = 0;
uint16_t Read_nAUTO_TEST     = 0;

void test_pins(void) {
  // start ADC conversions
//HAL_ADC_Stop_DMA(&hadc1);
//HAL_ADC_Start_DMA(&hadc1, (uint32_t *) ADC1_Results, ADC1_RESULTS_SIZE);

  HAL_GPIO_WritePin(AUX_GPIO_Port,        AUX_Pin,        Set_AUX        ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DEBUG_PE12_GPIO_Port, DEBUG_PE12_Pin, Set_DEBUG_PE12 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DEBUG_PE14_GPIO_Port, DEBUG_PE14_Pin, Set_DEBUG_PE14 ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(DEBUG_PE15_GPIO_Port, DEBUG_PE15_Pin, Set_DEBUG_PE15 ? GPIO_PIN_SET : GPIO_PIN_RESET);

  Read_IR_PGOOD       = HAL_GPIO_ReadPin(IR_PGOOD_GPIO_Port,       IR_PGOOD_Pin)       ? 1 : 0;
  Read_V3_3_PGOOD     = HAL_GPIO_ReadPin(V3_3_PGOOD_GPIO_Port,     V3_3_PGOOD_Pin)     ? 1 : 0;
  Read_PWR_IN_FAULT_A = HAL_GPIO_ReadPin(PWR_IN_FAULT_A_GPIO_Port, PWR_IN_FAULT_A_Pin) ? 1 : 0;
  Read_PWR_IN_FAULT_B = HAL_GPIO_ReadPin(PWR_IN_FAULT_B_GPIO_Port, PWR_IN_FAULT_B_Pin) ? 1 : 0;
  Read_nAUTO_TEST     = HAL_GPIO_ReadPin(nAUTO_TEST_GPIO_Port,     nAUTO_TEST_Pin)     ? 1 : 0;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
//------------------------------------------------------------------------------
// this is called first thing at beginning of main()
// void UserCodeBegin1(void) {
  // Disable lazy stacking for ARM errata 776924 [STM32L4x6xx Errata page 16]
  FPU->FPCCR &= ~FPU_FPCCR_LSPEN_Msk;         // Disable lazy stacking for floating point
  RCC->CSR   |= RCC_CSR_RMVF;                 // Clear fault flags so that we can tell why we reset on next reset

  // reset peripherals
  __HAL_RCC_AHB1_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB1_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_AHB2_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB2_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_AHB3_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB3_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_APB1_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_APB1_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_APB2_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_APB2_RELEASE_RESET(); __DSB(); __ISB(); 

  // Reset EXTI module manually as it is not included in APB2
  EXTI->IMR   =  0;     // Reset value
  EXTI->EMR   =  0;     // Reset value
  EXTI->RTSR  =  0;     // Reset value
  EXTI->FTSR  =  0;     // Reset value
  EXTI->SWIER =  0;     // Reset value
  EXTI->PR    = ~0;     // Reset value undefined! Write ones to clear.

  //----------------------------------------------------------------------------
  // setup for SYNC_OUT_IN pin function (1kHz sync pulse with EXTI interrupt on the same pin)
  // 
  // make PWM sync out on PE9 using timer 1 channel 1:
  //   prescaler 3 = 4-1
  //   period = 44999
  //   pulse = 22500
  //   PWM output mode 1
  // name pin PE9 SYNC_OUT_IN
  // 
  // to force generation of EXTI interrupt handler:
  //   define unused pin PB9 as a dummy pin as external interrupt input with pulldown
  //   name pin PB9 EXTI_dummy_nc
  //   enable EXTI line[9:5] interrupts
  //   for code generation / call HAL handler, uncheck the box (don't call handler)
  // 
  // to add the EXTI edge interrupt capability to T1.1 output:
  //   in HAL_TIM_MspPostInit(), add the following user code:
  //     // EXTI interrupt on Sync_1kHz_out_in
  //     GPIO_InitStruct.Mode |= GPIO_MODE_IT_RISING;
  //     HAL_GPIO_Init(SYNC_OUT_IN_GPIO_Port, &GPIO_InitStruct);
  // 
  // end setup for SYNC_OUT_IN pin function

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
  MX_TIM1_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_CRC_Init();
  /* USER CODE BEGIN 2 */

  Detector_Uart1_Init();

  // enable power to the detector board
  HAL_GPIO_WritePin(EN_DET_PCB_PWR_GPIO_Port, EN_DET_PCB_PWR_Pin, GPIO_PIN_SET);

  // compute code image CRCs so we can report them to detector
  Compute_code_image_CRCs();

  // emitter DMA request is triggered by the compare event  
  __HAL_TIM_ENABLE_DMA(&htim1, TIM_DMA_CC1 | TIM_DMA_CC2);

  // debug code to allow us to see the start of initialization on the logic analyzer
  IR_EN0_GPIO_Port ->ODR = 0xFFFF;
  IR_EN16_GPIO_Port->ODR = 0xFFFF;

  // start timer 1
  // timer 1 causes the emitter code to advance on update interrupt (via DMA to GPIO C and D)
  // channel 1, 2, and 3 are identical so that the two DMA streams are running off of channel 1 and 2
  // but we can't see the triggers
  // so we use channel 3 which is identical to channel 1 and 2,
  // which allows us to see the DMA triggers on PE13
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);     // trigger DMA 2 stream 1, internal trigger, not visible on port pin
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);     // trigger DMA 2 stream 2, internal trigger, not visible on port pin
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);     // timer 2 ch 3 trigger output on PE13 so we can see with the logic analyzer
  TIM1->CR1 &= ~TIM_CR1_CEN;                    // halt the timer until next sync pulse rising edge
  TIM1->CNT  = 0;

  memset(Emitter_Select_Codes_0_15,  0, sizeof(Emitter_Select_Codes_0_15));
  memset(Emitter_Select_Codes_16_31, 0, sizeof(Emitter_Select_Codes_16_31));

  // start timer 2
  // timer 2 is used to generate sync pulse used by emitter and detector
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);     // timer 2 ch 2 sync output on PA1

  // this pin has been reworked to tie directly to U404.EN
  // and U404.VSEL has been reworked to tie directly to watchdog output
  // so that when watchdog is not happy, U404.VSEL is zero,
  // thereby selecting a preprogrammed minimum value of 0.7V
  // which is not high enough to keep the emitters on
  // and when the watchdog is happy, U404.VSEL is one
  // thereby selecting the preprogrammed desired set value (nominally 2.0V)
  HAL_GPIO_WritePin(IR_LED_PWR_EN_GPIO_Port, IR_LED_PWR_EN_Pin, GPIO_PIN_SET);  // ???

  // enable emitter power watchdog
  HAL_GPIO_WritePin(EN_IR_LED_PWR_GPIO_Port, EN_IR_LED_PWR_Pin, GPIO_PIN_SET);  // ???

  EmittersInitialized = 1;

  // enable light show power supply
  HAL_GPIO_WritePin(RGB_PWR_EN_GPIO_Port, RGB_PWR_EN_Pin, GPIO_PIN_SET);

  // uncomment to blow away secondary code image
  Erase_flash_sector(FLASH_SECTOR_7);           // 128 kb

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    static int32_t target_tick = 0;
    if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
      target_tick = HAL_GetTick() + 3000;

      // test unused pins
      test_pins();
      }
    Emitter_PWM_task();
    Battery_monitor_task();
    RX_ParseMsgTask(&Detector_Uart1, HAL_GetTick());
    Emitter_code_image_task();
    LightShowTask();

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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Activate the Over-Drive mode 
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */
  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion) 
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = ENABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 3;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SEQ_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_112CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
  */
  sConfig.Channel = ADC_CHANNEL_8;
  sConfig.Rank = 2;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time. 
  */
  sConfig.Channel = ADC_CHANNEL_9;
  sConfig.Rank = 3;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief CRC Initialization Function
  * @param None
  * @retval None
  */
static void MX_CRC_Init(void)
{

  /* USER CODE BEGIN CRC_Init 0 */

  /* USER CODE END CRC_Init 0 */

  /* USER CODE BEGIN CRC_Init 1 */

  /* USER CODE END CRC_Init 1 */
  hcrc.Instance = CRC;
  if (HAL_CRC_Init(&hcrc) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CRC_Init 2 */

  /* USER CODE END CRC_Init 2 */

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
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 14;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM2;
  sConfigOC.Pulse = 8;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  htim2.Init.Prescaler = 3;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 22499;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
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
  sConfigOC.Pulse = 11250;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/** 
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void) 
{
  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);
  /* DMA2_Stream3_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream3_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, IR_EN13_Pin|IR_EN14_Pin|IR_EN15_Pin|IR_EN0_Pin 
                          |IR_EN1_Pin|IR_EN2_Pin|IR_EN3_Pin|IR_EN4_Pin 
                          |IR_EN5_Pin|IR_EN6_Pin|IR_EN7_Pin|IR_EN8_Pin 
                          |IR_EN9_Pin|IR_EN10_Pin|IR_EN11_Pin|IR_EN12_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, AUX_Pin|IR_LED_PWR_WD_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, DEBUG_PE12_Pin|DEBUG_PE14_Pin|DEBUG_PE15_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, IR_LED_PWR_EN_Pin|EN_IR_LED_PWR_Pin|RGB_PWR_EN_Pin|EN_DET_PCB_PWR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, IR_EN24_Pin|IR_EN25_Pin|IR_EN26_Pin|IR_EN27_Pin 
                          |IR_EN28_Pin|IR_EN29_Pin|IR_EN30_Pin|IR_EN31_Pin 
                          |IR_EN16_Pin|IR_EN17_Pin|IR_EN18_Pin|IR_EN19_Pin 
                          |IR_EN20_Pin|IR_EN21_Pin|IR_EN22_Pin|IR_EN23_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : IR_EN13_Pin IR_EN14_Pin IR_EN15_Pin IR_EN0_Pin 
                           IR_EN1_Pin IR_EN2_Pin IR_EN3_Pin IR_EN4_Pin 
                           IR_EN5_Pin IR_EN6_Pin IR_EN7_Pin IR_EN8_Pin 
                           IR_EN9_Pin IR_EN10_Pin IR_EN11_Pin IR_EN12_Pin */
  GPIO_InitStruct.Pin = IR_EN13_Pin|IR_EN14_Pin|IR_EN15_Pin|IR_EN0_Pin 
                          |IR_EN1_Pin|IR_EN2_Pin|IR_EN3_Pin|IR_EN4_Pin 
                          |IR_EN5_Pin|IR_EN6_Pin|IR_EN7_Pin|IR_EN8_Pin 
                          |IR_EN9_Pin|IR_EN10_Pin|IR_EN11_Pin|IR_EN12_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : AUX_Pin IR_LED_PWR_WD_Pin */
  GPIO_InitStruct.Pin = AUX_Pin|IR_LED_PWR_WD_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_PGOOD_Pin V3_3_PGOOD_Pin PWR_IN_FAULT_B_Pin PWR_IN_FAULT_A_Pin 
                           nAUTO_TEST_Pin */
  GPIO_InitStruct.Pin = IR_PGOOD_Pin|V3_3_PGOOD_Pin|PWR_IN_FAULT_B_Pin|PWR_IN_FAULT_A_Pin 
                          |nAUTO_TEST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : SYNC_IN_Pin */
  GPIO_InitStruct.Pin = SYNC_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SYNC_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : DEBUG_PE12_Pin DEBUG_PE14_Pin DEBUG_PE15_Pin */
  GPIO_InitStruct.Pin = DEBUG_PE12_Pin|DEBUG_PE14_Pin|DEBUG_PE15_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_LED_PWR_EN_Pin EN_IR_LED_PWR_Pin RGB_PWR_EN_Pin EN_DET_PCB_PWR_Pin */
  GPIO_InitStruct.Pin = IR_LED_PWR_EN_Pin|EN_IR_LED_PWR_Pin|RGB_PWR_EN_Pin|EN_DET_PCB_PWR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : IR_EN24_Pin IR_EN25_Pin IR_EN26_Pin IR_EN27_Pin 
                           IR_EN28_Pin IR_EN29_Pin IR_EN30_Pin IR_EN31_Pin 
                           IR_EN16_Pin IR_EN17_Pin IR_EN18_Pin IR_EN19_Pin 
                           IR_EN20_Pin IR_EN21_Pin IR_EN22_Pin IR_EN23_Pin */
  GPIO_InitStruct.Pin = IR_EN24_Pin|IR_EN25_Pin|IR_EN26_Pin|IR_EN27_Pin 
                          |IR_EN28_Pin|IR_EN29_Pin|IR_EN30_Pin|IR_EN31_Pin 
                          |IR_EN16_Pin|IR_EN17_Pin|IR_EN18_Pin|IR_EN19_Pin 
                          |IR_EN20_Pin|IR_EN21_Pin|IR_EN22_Pin|IR_EN23_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
