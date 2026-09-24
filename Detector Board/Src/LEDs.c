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

  memset(&StatusLightData[                         0], LED_zero_bit, NUMBER_OF_STATUS_LED_BYTES);
  memset(&StatusLightData[NUMBER_OF_STATUS_LED_BYTES],            0, NUMBER_OF_RESET_BYTES);
}

//------------------------------------------------------------------------------
extern SPI_HandleTypeDef hspi3;
extern SPI_HandleTypeDef hspi2;
#define LED_SPI          hspi3
#define STATUS_LED_SPI   hspi2

//------------------------------------------------------------------------------
volatile bool LED_tx_busy        = false;
volatile bool LED_Status_tx_busy = false;

//------------------------------------------------------------------------------
// start new LED DMA transfer
void Start_LEDs_tx(void) {
  LED_tx_busy = true;
//HAL_SPI_DMAStop(&LED_SPI);
  HAL_SPI_Transmit_DMA(&LED_SPI, LightShowData, sizeof(LightShowData));
}

//------------------------------------------------------------------------------
void ShowStatusLEDs(void) {
  LED_Status_tx_busy = true;
//HAL_SPI_DMAStop(&STATUS_LED_SPI);
  HAL_SPI_Transmit_DMA(&STATUS_LED_SPI, StatusLightData, sizeof(StatusLightData));
}

//------------------------------------------------------------------------------
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if      (hspi == &LED_SPI)        LED_tx_busy        = false;
  else if (hspi == &STATUS_LED_SPI) LED_Status_tx_busy = false;
}

//------------------------------------------------------------------------------
#define StatusLED_intensity 4
void StatusLED_test(void) {
  static uint16_t Status_LED_Index = 0;

  // assure previous LED tx is complete before starting another
  if (LED_Status_tx_busy) return;

  SetStatusLED((Status_LED_Index+0) & 0x03, StatusLED_intensity,                   0, 0);
  SetStatusLED((Status_LED_Index+1) & 0x03,                   0, StatusLED_intensity, 0);
  SetStatusLED((Status_LED_Index+2) & 0x03,                   0,                   0, StatusLED_intensity);
  SetStatusLED((Status_LED_Index+3) & 0x03, StatusLED_intensity, StatusLED_intensity, StatusLED_intensity);
  Status_LED_Index++;

  ShowStatusLEDs();
}

//------------------------------------------------------------------------------
// flash timer resolution is 100 ms
// 0=blue 1=green 2=red 3=yellow
RGB_indicators_t RGB_indicators = {
  // layer 2 is controlled by the target
  // layer 2 is on top and overlays layer 1
  // layer 2a is always "all LEDs" (0xffffffff)
  // layer 2b is "the smear" (ball track entering the light curtain through exiting the light curtain, and all points in between)
  .LEDs_layer_2          = 0,                   // layer 2 bitmap, left-most bits are top
  .RGB_layer_2           = 0,                   // layer 2 color

  // layer 1 is controlled by the app
  // layer 1 is on the bottom below layer 2
  .LEDs1_on              = 0x00000000,          // leds on
  .LEDs1_blink           = 0x00000000,          // leds blink
  .RGB1                  = (255ul << 16) |      // red
                           (  0ul <<  8) |      // green
                           (  0ul <<  0),       // blue
  .blink_on_time_ms      = 1000,                // blink  on-time in ms
  .blink_off_time_ms     = 1000,                // blink off-time in ms
  .ball_crossing_time_ms = 300,                 // ball-crossing time in 1 ms tick
  .ball_crossing_event   = 0                    // ball-crossing event
};

//------------------------------------------------------------------------------
// show layer 2 light curtain event independently from layer 1
// object-type: 0=non-ball 1=ball
#define clWhite   0x00fffffful
#define clBlack   0x00000000ul
#define clOrange  ((128ul << 16) | (50ul << 8) | (  0ul << 0))   // RGB (128, 50,   0) = orange
#define clPurple  ((128ul << 16) | ( 0ul << 8) | (128ul << 0))   // RGB (128,  0, 128) = purple
void LEDs_layer2_task(bool ball_crossing_event, uint16_t object_type, bool elevation_within_zone, uint32_t vector) {
  static uint32_t task_timer;
  static  int16_t state  = 0;
  static uint32_t LEDs2a = 0;           // layer 2a LEDs are always all-ones
  static uint32_t LEDs2b = 0;           // layer 2b LEDs (t
  static uint32_t RGB2a  = 0;           // layer 2a color
  static uint32_t RGB2b  = 0;           // layer 2b color

  // if new crossing event, start (or restart) layer 2
  if (ball_crossing_event) {
    state = 0;
    }

  switch (state) {
    case -1: // showing layer 2b, wait for timeout
             if ((int32_t) (task_timer - HAL_GetTick()) > 0) break;
             state++;
    case  0: // init layer 2 to "off"
             RGB_indicators.LEDs_layer_2 = 0ul;
             RGB_indicators.RGB_layer_2  = clBlack;
             state++;
    case  1: // idle, not showing layer 2
             if (vector                  == 0ul)   break;       // must have a ball or non-ball crossing
           //if (RGB_indicators.LEDs1_on == 0ul)   break;       // target must be lit to show crossing indication
           //if (elevation_within_zone   == false) break;       // if crossing is outside of lit zone, show nothing

             if (object_type) {
               // ball: show all white followed by a white smear
             //LEDs2a = RGB_indicators.LEDs1_on;                // first vector is on only where the target is lit up
               if (elevation_within_zone) {
                 LEDs2a = RGB_indicators.LEDs1_on;              // ball inside  the zone: first vector is on only where the target is lit up 
                 LEDs2b = vector;                               // second vector is white smear only
                 RGB2a  = clWhite;
                 RGB2b  = clWhite;
                 }
               else {
                 LEDs2a = vector;                               // ball outside the zone: first vector is orange smear only
                 LEDs2b = vector;                               // second vector is orange smear only
                 RGB2a  = clOrange;
                 RGB2b  = clOrange;
                 }
               }
             else {
               // non-ball: show orange smear followed by a orange smear
               LEDs2a = vector;                                 // first vector is orange smear only
               LEDs2b = vector;                                 // second vector is orange smear only
               RGB2a  = clOrange;
               RGB2b  = clOrange;
               }

             // start showing layer 2a
             RGB_indicators.LEDs_layer_2 = LEDs2a;
             RGB_indicators.RGB_layer_2  = RGB2a;

             // show layer 2a for 300 ms
             task_timer = HAL_GetTick() + RGB_indicators.ball_crossing_time_ms;
             state++;
    case  2: // showing layer 2a, wait for timeout
             if ((int32_t) (task_timer - HAL_GetTick()) > 0) break;

             // start showing layer 2b
             RGB_indicators.LEDs_layer_2 = LEDs2b;
             RGB_indicators.RGB_layer_2  = RGB2b;

             // show layer 2b for 600 ms
             task_timer = HAL_GetTick() + 600;
             state = -1;
    }
}

//------------------------------------------------------------------------------
// blink has higher priority than on/off
uint16_t Send_EmitterSetLEDs_Msg_fill_count  = 0;
uint16_t Send_EmitterSetLEDs_Msg_empty_count = 0;
void LightShow_RGB_Indicator_control_task(void) {
//static int16_t  blink_state = 0;      // blink state machine
//static uint32_t target_tick;          // used for counting blink on and off time
  static uint32_t LEDs2       = 0;      // layer 2 LEDs
//static uint32_t LEDs2b      = 0;      // layer 2b LEDs
  static uint32_t RGB2        = 0;      // layer 2 color
//static uint32_t RGB2b       = 0;      // layer 2b color
  static uint32_t LEDs1       = 0;      // layer 1 LEDs that are currently displayed
  static uint32_t RGB1        = 0;      // layer 1 color
         uint32_t LEDs        = 0;      // desired layer 1 LEDs
         uint32_t RGB         = 0;      // desired layer 1 color
         uint32_t mask;
          int16_t i;

  // check for changes in layer 2
  LEDs_layer2_task(false, 0, false, 0);

  // notify emitters of LED change
  // emitter LEDs follow detector LEDs
  if ((int16_t) (Send_EmitterSetLEDs_Msg_fill_count - Send_EmitterSetLEDs_Msg_empty_count) > 0) {
    EmitterSetLEDsMsg.LEDs2                 = LEDs2;
    EmitterSetLEDsMsg.RGB2                  = RGB2;
    EmitterSetLEDsMsg.LEDs1                 = LEDs1;
    EmitterSetLEDsMsg.RGB1                  = RGB1;
    EmitterSetLEDsMsg.ball_crossing_time_ms = RGB_indicators.ball_crossing_time_ms;

    Send_EmitterSetLEDs_Msg_empty_count += Send_EmitterSetLEDsMsg();
    }

  LEDs = RGB_indicators.LEDs1_on;
  RGB  = RGB_indicators.RGB1;

  #if 0
  // implement blink
  switch (blink_state) {
    case -1: // LEDs are off for ball crossing event
             LEDs = 0;
             if (((int32_t) (target_tick - HAL_GetTick()) > 0)) break;
             blink_state++;
    case  0: // idle, no blinking
             if (RGB_indicators.ball_crossing_event) {
               // turn all LEDs off at ball crossing
               RGB_indicators.ball_crossing_event =  0;
               LEDs                               =  0;
               blink_state                        = -1;
               target_tick                        = HAL_GetTick() + RGB_indicators.ball_crossing_time_ms;
               break;
               }
             else if (RGB_indicators.LEDs1_blink == 0) break;
             // start flashing blink LEDs
             target_tick = HAL_GetTick();
             blink_state++;
    case  1: // blink LEDs are off, wait for blink-off timer to expire
             LEDs &= ~RGB_indicators.LEDs1_blink;
             if (((int32_t) (target_tick - HAL_GetTick()) > 0)) break;
             target_tick += RGB_indicators.blink_on_time_ms;                    // turn blink LEDs on
             blink_state++;
    case  2: // blink LEDs are on, wait for blink-on timer to expire
             LEDs |= RGB_indicators.LEDs1_blink;
             if (((int32_t) (target_tick - HAL_GetTick()) > 0)) break;
             if (RGB_indicators.LEDs1_blink) {
               target_tick += RGB_indicators.blink_off_time_ms;
               blink_state = 1;
               }
             else blink_state = 0;
             break;
    }
  #endif

  // if any layer 2 LEDs are on, turn off all layer 1 LEDs
  // if (RGB_indicators.LEDs_layer_2) LEDs = 0;

  if ((LEDs1 != LEDs)                        ||
      (RGB1  != RGB)                         ||
      (LEDs2 != RGB_indicators.LEDs_layer_2) ||
      (RGB2  != RGB_indicators.RGB_layer_2)) {
    LEDs1 = LEDs;
    RGB1  = RGB;
    LEDs2 = RGB_indicators.LEDs_layer_2;
    RGB2  = RGB_indicators.RGB_layer_2;

    // notify emitter to update LEDs
    Send_EmitterSetLEDs_Msg_fill_count++;
    
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
