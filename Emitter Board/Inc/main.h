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
#define IR_EN13_Pin GPIO_PIN_13
#define IR_EN13_GPIO_Port GPIOC
#define IR_EN14_Pin GPIO_PIN_14
#define IR_EN14_GPIO_Port GPIOC
#define IR_EN15_Pin GPIO_PIN_15
#define IR_EN15_GPIO_Port GPIOC
#define IR_EN0_Pin GPIO_PIN_0
#define IR_EN0_GPIO_Port GPIOC
#define IR_EN1_Pin GPIO_PIN_1
#define IR_EN1_GPIO_Port GPIOC
#define IR_EN2_Pin GPIO_PIN_2
#define IR_EN2_GPIO_Port GPIOC
#define IR_EN3_Pin GPIO_PIN_3
#define IR_EN3_GPIO_Port GPIOC
#define AUX_Pin GPIO_PIN_0
#define AUX_GPIO_Port GPIOA
#define SYNC_OUT_Pin GPIO_PIN_1
#define SYNC_OUT_GPIO_Port GPIOA
#define TO_PC_Pin GPIO_PIN_2
#define TO_PC_GPIO_Port GPIOA
#define FROM_PC_Pin GPIO_PIN_3
#define FROM_PC_GPIO_Port GPIOA
#define DET_PCB_IMON_Pin GPIO_PIN_6
#define DET_PCB_IMON_GPIO_Port GPIOA
#define RGB_SER_DATA_3V_Pin GPIO_PIN_7
#define RGB_SER_DATA_3V_GPIO_Port GPIOA
#define IR_EN4_Pin GPIO_PIN_4
#define IR_EN4_GPIO_Port GPIOC
#define IR_EN5_Pin GPIO_PIN_5
#define IR_EN5_GPIO_Port GPIOC
#define BAT_IMON_Pin GPIO_PIN_0
#define BAT_IMON_GPIO_Port GPIOB
#define BAT_VMON_Pin GPIO_PIN_1
#define BAT_VMON_GPIO_Port GPIOB
#define IR_PGOOD_Pin GPIO_PIN_7
#define IR_PGOOD_GPIO_Port GPIOE
#define V3_3_PGOOD_Pin GPIO_PIN_8
#define V3_3_PGOOD_GPIO_Port GPIOE
#define SYNC_IN_Pin GPIO_PIN_9
#define SYNC_IN_GPIO_Port GPIOE
#define SYNC_IN_EXTI_IRQn EXTI9_5_IRQn
#define PWR_IN_FAULT_B_Pin GPIO_PIN_10
#define PWR_IN_FAULT_B_GPIO_Port GPIOE
#define PWR_IN_FAULT_A_Pin GPIO_PIN_11
#define PWR_IN_FAULT_A_GPIO_Port GPIOE
#define DEBUG_PE12_Pin GPIO_PIN_12
#define DEBUG_PE12_GPIO_Port GPIOE
#define Emitter_DMA_trigger_Pin GPIO_PIN_13
#define Emitter_DMA_trigger_GPIO_Port GPIOE
#define DEBUG_PE14_Pin GPIO_PIN_14
#define DEBUG_PE14_GPIO_Port GPIOE
#define DEBUG_PE15_Pin GPIO_PIN_15
#define DEBUG_PE15_GPIO_Port GPIOE
#define IR_LED_PWR_EN_Pin GPIO_PIN_12
#define IR_LED_PWR_EN_GPIO_Port GPIOB
#define EN_IR_LED_PWR_Pin GPIO_PIN_13
#define EN_IR_LED_PWR_GPIO_Port GPIOB
#define RGB_PWR_EN_Pin GPIO_PIN_14
#define RGB_PWR_EN_GPIO_Port GPIOB
#define IR_EN24_Pin GPIO_PIN_8
#define IR_EN24_GPIO_Port GPIOD
#define IR_EN25_Pin GPIO_PIN_9
#define IR_EN25_GPIO_Port GPIOD
#define IR_EN26_Pin GPIO_PIN_10
#define IR_EN26_GPIO_Port GPIOD
#define IR_EN27_Pin GPIO_PIN_11
#define IR_EN27_GPIO_Port GPIOD
#define IR_EN28_Pin GPIO_PIN_12
#define IR_EN28_GPIO_Port GPIOD
#define IR_EN29_Pin GPIO_PIN_13
#define IR_EN29_GPIO_Port GPIOD
#define IR_EN30_Pin GPIO_PIN_14
#define IR_EN30_GPIO_Port GPIOD
#define IR_EN31_Pin GPIO_PIN_15
#define IR_EN31_GPIO_Port GPIOD
#define IR_EN6_Pin GPIO_PIN_6
#define IR_EN6_GPIO_Port GPIOC
#define IR_EN7_Pin GPIO_PIN_7
#define IR_EN7_GPIO_Port GPIOC
#define IR_EN8_Pin GPIO_PIN_8
#define IR_EN8_GPIO_Port GPIOC
#define IR_EN9_Pin GPIO_PIN_9
#define IR_EN9_GPIO_Port GPIOC
#define IR_LED_PWR_WD_Pin GPIO_PIN_8
#define IR_LED_PWR_WD_GPIO_Port GPIOA
#define TO_DETECTOR_MCU_Pin GPIO_PIN_9
#define TO_DETECTOR_MCU_GPIO_Port GPIOA
#define FROM_DETECTOR_MCU_Pin GPIO_PIN_10
#define FROM_DETECTOR_MCU_GPIO_Port GPIOA
#define IR_EN10_Pin GPIO_PIN_10
#define IR_EN10_GPIO_Port GPIOC
#define IR_EN11_Pin GPIO_PIN_11
#define IR_EN11_GPIO_Port GPIOC
#define IR_EN12_Pin GPIO_PIN_12
#define IR_EN12_GPIO_Port GPIOC
#define IR_EN16_Pin GPIO_PIN_0
#define IR_EN16_GPIO_Port GPIOD
#define IR_EN17_Pin GPIO_PIN_1
#define IR_EN17_GPIO_Port GPIOD
#define IR_EN18_Pin GPIO_PIN_2
#define IR_EN18_GPIO_Port GPIOD
#define IR_EN19_Pin GPIO_PIN_3
#define IR_EN19_GPIO_Port GPIOD
#define IR_EN20_Pin GPIO_PIN_4
#define IR_EN20_GPIO_Port GPIOD
#define IR_EN21_Pin GPIO_PIN_5
#define IR_EN21_GPIO_Port GPIOD
#define IR_EN22_Pin GPIO_PIN_6
#define IR_EN22_GPIO_Port GPIOD
#define IR_EN23_Pin GPIO_PIN_7
#define IR_EN23_GPIO_Port GPIOD
#define EN_DET_PCB_PWR_Pin GPIO_PIN_5
#define EN_DET_PCB_PWR_GPIO_Port GPIOB
#define IR_LED_SCL_Pin GPIO_PIN_6
#define IR_LED_SCL_GPIO_Port GPIOB
#define nAUTO_TEST_Pin GPIO_PIN_1
#define nAUTO_TEST_GPIO_Port GPIOE
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
