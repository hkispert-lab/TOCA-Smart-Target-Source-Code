#ifndef __LEDS_H
#define __LEDS_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  // layer 2 is controlled by the target
  // layer 2 is on top and overlays layer 1
  // layer 2a is always "all LEDs" (0xffffffff)
  // layer 2b is "the smear" (ball track entering the light curtain through exiting the light curtain, and all points in between)
  uint32_t LEDs_layer_2;                // layer 2 bitmap, left-most bits are top
  uint32_t RGB_layer_2;                 // layer 2 color

  // layer 1, controlled by app
  // layer 1 is on the bottom below layer 2
  uint32_t LEDs1_on;                    // layer 1 bitmap, left-most bits are top
  uint32_t LEDs1_blink;
  uint32_t RGB1;                        // layer 1 color
  uint16_t blink_on_time_ms;            // blink on-time in 1 ms tick
  uint16_t blink_off_time_ms;           // blink off-time in 1 ms tick
  uint16_t ball_crossing_time_ms;       // ball-crossing time in 1 ms tick
  uint16_t ball_crossing_event;         // non-zero is ball-crossing event
} RGB_indicators_t;

extern RGB_indicators_t RGB_indicators;

extern          int16_t  LightShow_Selector;
extern          bool     LightShow_Init;
extern volatile bool     LED_tx_busy;
extern          uint32_t LEDs_red;
extern          uint32_t LEDs_green;
extern          uint32_t LEDs_blue;
extern          uint16_t POST_state;
extern          uint16_t LightShow_POST_update_request;

void LED_Data_Init(void);
void Start_LEDs_tx(void);
void LEDs_layer2_task(bool ball_crossing_event, uint16_t object_type, bool elevation_within_zone, uint32_t vector);
void LightShow_RGB_Indicator_control_task(void);
void LightShowTask(void);
void StatusLED_test(void);

#endif
