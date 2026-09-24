// LEDs.c
// this is the light show

#include <string.h>
//#include <stdlib.h>
//#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "UART.h"
#include "LEDsCommon.h"
#include "LEDs.h"

//------------------------------------------------------------------------------
// for testing OTA update
// we create several builds that light up the LEDs in different ways
// select the desired light show effect
// else init to e_LightShow_Off for no light show effect
int16_t LightShow_Selector_test = 
    e_LightShow_Off;                      //  0
//  e_LightShow_ColorWipe;                //  1
//  e_LightShow_FadeInandFadeOutRGB;      //  2
//  e_LightShow_Sparkle;                  //  3
//  e_LightShow_Fire;                     //  4
//  e_LightShow_MeteorRain;               //  5
//  e_LightShow_RunningLights;            //  6
//  e_LightShow_Breathe;                  //  7
//  e_LightShow_Red;                      // 16
//  e_LightShow_Green;                    // 17
//  e_LightShow_Blue;                     // 18

//------------------------------------------------------------------------------
void LED_Data_Init(void) {
  memset(&LightShowData[                  0], LED_zero_bit, NUMBER_OF_LED_BYTES);
  memset(&LightShowData[NUMBER_OF_LED_BYTES],            0, NUMBER_OF_RESET_BYTES);
}

//------------------------------------------------------------------------------
extern SPI_HandleTypeDef hspi1;
#define LED_SPI          hspi1

//------------------------------------------------------------------------------
volatile bool LED_tx_busy = false;

//------------------------------------------------------------------------------
// start new LED DMA transfer
void Start_LEDs_tx(void) {
  LED_tx_busy = true;
//HAL_SPI_DMAStop(&LED_SPI);
  HAL_SPI_Transmit_DMA(&LED_SPI, LightShowData, sizeof(LightShowData));
}

//------------------------------------------------------------------------------
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi == &LED_SPI) LED_tx_busy = false;
}

//------------------------------------------------------------------------------
void LightShow_RGB_Indicator_control_task(void) {
  if ((int16_t) (EmitterSetLEDs_Msg_fill_count - EmitterSetLEDs_Msg_empty_count) > 0) {
    uint32_t mask;
    int16_t  i;

    #define LEDs2 EmitterSetLEDs_Msg.LEDs2
    #define RGB2  EmitterSetLEDs_Msg.RGB2 
    #define LEDs1 EmitterSetLEDs_Msg.LEDs1
    #define RGB1  EmitterSetLEDs_Msg.RGB1 

    EmitterSetLEDs_Msg_empty_count++;
    mask = (uint32_t) 1 << 31;
    for (i = 0; i < NUMBER_OF_LEDs; i++) {
      if      (LEDs2 & mask) SetPixel(i, (RGB2 >> 16) & 0xff, (RGB2 >> 8) & 0xff, (RGB2 >> 0) & 0xff);
      else if (LEDs1 & mask) SetPixel(i, (RGB1 >> 16) & 0xff, (RGB1 >> 8) & 0xff, (RGB1 >> 0) & 0xff);
      else                   SetPixel(i,                   0,                  0,                  0);
      mask >>= 1;
      }

    Start_LEDs_tx();
    }
}
