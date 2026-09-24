/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define AUX_Pin GPIO_PIN_5
#define AUX_GPIO_Port GPIOF
#define DET_PCB_IMON_Pin GPIO_PIN_7
#define DET_PCB_IMON_GPIO_Port GPIOF
#define RGB_SER_DATA_3V_Pin GPIO_PIN_2
#define RGB_SER_DATA_3V_GPIO_Port GPIOB
#define V5_PGOOD_Pin GPIO_PIN_12
#define V5_PGOOD_GPIO_Port GPIOF
#define V3_3_PGOOD_Pin GPIO_PIN_15
#define V3_3_PGOOD_GPIO_Port GPIOF
#define ADC_DMA_half_complete_Pin GPIO_PIN_1
#define ADC_DMA_half_complete_GPIO_Port GPIOG
#define FROM_PC_Pin GPIO_PIN_7
#define FROM_PC_GPIO_Port GPIOE
#define TO_PC_Pin GPIO_PIN_8
#define TO_PC_GPIO_Port GPIOE
#define SYNC_IN_Pin GPIO_PIN_9
#define SYNC_IN_GPIO_Port GPIOE
#define SYNC_IN_EXTI_IRQn EXTI9_5_IRQn
#define FRAM_nCS_Pin GPIO_PIN_15
#define FRAM_nCS_GPIO_Port GPIOE
#define EN_RGB_PWR_Pin GPIO_PIN_14
#define EN_RGB_PWR_GPIO_Port GPIOB
#define UI_SER_DATA_3V_Pin GPIO_PIN_15
#define UI_SER_DATA_3V_GPIO_Port GPIOB
#define TIME_TEST_Pin GPIO_PIN_9
#define TIME_TEST_GPIO_Port GPIOD
#define ADC_DMA_complete_Pin GPIO_PIN_2
#define ADC_DMA_complete_GPIO_Port GPIOG
#define DEBUG_PG3_Pin GPIO_PIN_3
#define DEBUG_PG3_GPIO_Port GPIOG
#define DEBUG_PG4_Pin GPIO_PIN_4
#define DEBUG_PG4_GPIO_Port GPIOG
#define DEBUG_PG5_Pin GPIO_PIN_5
#define DEBUG_PG5_GPIO_Port GPIOG
#define DEBUG_PG6_Pin GPIO_PIN_6
#define DEBUG_PG6_GPIO_Port GPIOG
#define DEBUG_PG7_Pin GPIO_PIN_7
#define DEBUG_PG7_GPIO_Port GPIOG
#define DEBUG_PG8_Pin GPIO_PIN_8
#define DEBUG_PG8_GPIO_Port GPIOG
#define EN_nBIAS_Pin GPIO_PIN_8
#define EN_nBIAS_GPIO_Port GPIOC
#define EN_5V_PWR_Pin GPIO_PIN_9
#define EN_5V_PWR_GPIO_Port GPIOC
#define TO_EMITTER_MCU_Pin GPIO_PIN_9
#define TO_EMITTER_MCU_GPIO_Port GPIOA
#define FROM_EMITTER_MCU_Pin GPIO_PIN_10
#define FROM_EMITTER_MCU_GPIO_Port GPIOA
#define DEBUG_PG9_Pin GPIO_PIN_9
#define DEBUG_PG9_GPIO_Port GPIOG
#define DEBUG_PG10_Pin GPIO_PIN_10
#define DEBUG_PG10_GPIO_Port GPIOG
#define DEBUG_PG11_Pin GPIO_PIN_11
#define DEBUG_PG11_GPIO_Port GPIOG
#define DEBUG_PG12_Pin GPIO_PIN_12
#define DEBUG_PG12_GPIO_Port GPIOG
#define DEBUG_PG13_Pin GPIO_PIN_13
#define DEBUG_PG13_GPIO_Port GPIOG
#define DEBUG_PG14_Pin GPIO_PIN_14
#define DEBUG_PG14_GPIO_Port GPIOG
#define DEBUG_PG15_Pin GPIO_PIN_15
#define DEBUG_PG15_GPIO_Port GPIOG
#define nAUTO_TEST_Pin GPIO_PIN_1
#define nAUTO_TEST_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
