#ifndef __LEDS_H
#define __LEDS_H

#include <stdint.h>
#include <stdbool.h>

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
void LightShow_RGB_Indicator_control_task(void);
void LightShowTask(void);

#endif
