// LEDsCommon.c
// this is the light show common code

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "UART.h"
#include "LEDs.h"
#include "LEDsCommon.h"

//------------------------------------------------------------------------------
// SPI1 setup:
//
// Mode: Transmit Only Master
// Hardware NSS Signal Disable
// Parameter settings:
//      Frame Format Motorola
//      Data Size 8 bits
//      First Bit MSB First
//      Baud Rate 5.625 MBits/s
//      Clock Polarity (CPOL) Low
//      Clock Phase (CPHA) 1 edge
//      CRC Calculation Disabled
//      NSS Signal Type Software
// User constants none
// NVIC DMA2 stream3 global interrupt enabled
//      SPI global interrupt disabled
// DMA Settings SPI1_TX, DMA2 Stream 3, Memory To Peripheral, Low priority
// GPIO Settings:
//      PA5 SPI1_SCK  Alternate, no pullup and no pulldown, very high
//      PA7 SPI1_MOSI Alternate, no pullup and no pulldown, very high

//
// there are 32*24 + 56 = 824 bytes of Light Show data
// at 5.625 MBits / second, 824 bytes * (8 bits/byte) * (1 us / 5.625 bits) = 1171.911 us
// to transmit the entire string of LEDs via SPI
//
uint8_t LightShowData  [NUMBER_OF_LED_BYTES        + NUMBER_OF_RESET_BYTES];    // (num LEDs) * (3*8 bytes for GRB) + (56 output low bytes for reset)
uint8_t StatusLightData[NUMBER_OF_STATUS_LED_BYTES + NUMBER_OF_RESET_BYTES];    // (num LEDs) * (3*8 bytes for GRB) + (56 output low bytes for reset)

//------------------------------------------------------------------------------
// set pixel color
// LightShowData[index*3 + 0]   <- green color for this pixel
// LightShowData[index*3 + 1]   <- red   color for this pixel
// LightShowData[index*3 + 2]   <- blue  color for this pixel
//

void SetLED(uint8_t *GRB, uint16_t red, uint16_t green, uint16_t blue) {
  GRB[ 0] = (green & 0x80) ? LED_one_bit : LED_zero_bit;
  GRB[ 1] = (green & 0x40) ? LED_one_bit : LED_zero_bit;
  GRB[ 2] = (green & 0x20) ? LED_one_bit : LED_zero_bit;
  GRB[ 3] = (green & 0x10) ? LED_one_bit : LED_zero_bit;
  GRB[ 4] = (green & 0x08) ? LED_one_bit : LED_zero_bit;
  GRB[ 5] = (green & 0x04) ? LED_one_bit : LED_zero_bit;
  GRB[ 6] = (green & 0x02) ? LED_one_bit : LED_zero_bit;
  GRB[ 7] = (green & 0x01) ? LED_one_bit : LED_zero_bit;

  GRB[ 8] = (red   & 0x80) ? LED_one_bit : LED_zero_bit;
  GRB[ 9] = (red   & 0x40) ? LED_one_bit : LED_zero_bit;
  GRB[10] = (red   & 0x20) ? LED_one_bit : LED_zero_bit;
  GRB[11] = (red   & 0x10) ? LED_one_bit : LED_zero_bit;
  GRB[12] = (red   & 0x08) ? LED_one_bit : LED_zero_bit;
  GRB[13] = (red   & 0x04) ? LED_one_bit : LED_zero_bit;
  GRB[14] = (red   & 0x02) ? LED_one_bit : LED_zero_bit;
  GRB[15] = (red   & 0x01) ? LED_one_bit : LED_zero_bit;

  GRB[16] = (blue  & 0x80) ? LED_one_bit : LED_zero_bit;
  GRB[17] = (blue  & 0x40) ? LED_one_bit : LED_zero_bit;
  GRB[18] = (blue  & 0x20) ? LED_one_bit : LED_zero_bit;
  GRB[19] = (blue  & 0x10) ? LED_one_bit : LED_zero_bit;
  GRB[20] = (blue  & 0x08) ? LED_one_bit : LED_zero_bit;
  GRB[21] = (blue  & 0x04) ? LED_one_bit : LED_zero_bit;
  GRB[22] = (blue  & 0x02) ? LED_one_bit : LED_zero_bit;
  GRB[23] = (blue  & 0x01) ? LED_one_bit : LED_zero_bit;
}

//------------------------------------------------------------------------------
void SetStatusLED(uint16_t index, uint16_t red, uint16_t green, uint16_t blue) {
  SetLED(StatusLightData + index*24, red, green, blue); // 24 bytes per LED
}

//------------------------------------------------------------------------------
// index 0 is the top of the LED stack, index 31 is the bottom
void SetPixel(uint16_t index, uint16_t red, uint16_t green, uint16_t blue) {
  SetLED(LightShowData + index*24, red, green, blue); // 24 bytes per LED
}

//------------------------------------------------------------------------------
// index 0 is top, index 31 is bottom
void SetAllPixel(uint16_t red, uint16_t green, uint16_t blue) {
  int16_t i;

  for (i = 0; i < NUMBER_OF_LEDs; i++) SetPixel(i, red, green, blue);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// random(max)          // min is assumed to be zero
// random(min, max)
uint32_t random(uint32_t rmin, uint32_t rmax) {
  uint32_t span      = rmax - rmin;
  uint32_t randvalue = rand();

  float retval = (float) randvalue * (float) span / (float) RAND_MAX;

  return (uint32_t) retval + rmin;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void LightShow_ColorWipe(bool init) {
  static int16_t  state = -1;
  static int16_t  index;
  static uint32_t target_tick;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             index = 0;
             state++;
    case  0: // wait 50 ms for turn-on delay
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 50;                         // setup next delay
             SetPixel(index, 0, 255, 0);                // pixel on green
             if (++index >= NUMBER_OF_LEDs) {
               index = 0;
               state++;
               }
             Start_LEDs_tx();
             break;
    case  1: // wait 50 ms for turn-off delay
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 50;                         // setup next delay
             SetPixel(index, 0, 0, 0);                  // pixel off
             if (++index >= NUMBER_OF_LEDs) {
               target_tick += 3200;                     // setup dead time
               index = 0;
               state = 0;
               }
             Start_LEDs_tx();
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
void LightShow_FadeInandFadeOutRGB(bool init) {
  static  int16_t state = -1;
  static  int16_t intensity;
  static uint16_t RGB_select;
  static uint32_t target_tick;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             intensity   = 0;
             RGB_select  = 0;
             target_tick = HAL_GetTick();
             state++;
    case  0: // wait for fade-in  delay
    case  2: // wait for fade-out delay
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 3;
             switch (RGB_select) {
               case  0: SetAllPixel(intensity,         0,         0); break;
               case  1: SetAllPixel(        0, intensity,         0); break;
               case  2: SetAllPixel(        0,         0, intensity); break;
               }
             Start_LEDs_tx();
             state++;
             break;
    case  1: // fade-in adjustment
             if (++intensity < 255) state = 0;  // continue fade-in
             else                   state = 2;  // start fade-out
             break;
    case  3: // fade-out adjustment
             if (--intensity > 0)   state = 2;  // continue fade-out
             else {
               // done with fade out, start fade in with next color
               if (++RGB_select > 2) RGB_select = 0;
               state = 0;
               }
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define SPARKLE_INTENSITY 0x20
void LightShow_Sparkle(bool init) {
  static  int16_t state = -1;
  static uint16_t pixel_number;
  static uint32_t target_tick;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             SetAllPixel(SPARKLE_INTENSITY, SPARKLE_INTENSITY, SPARKLE_INTENSITY);
             target_tick = HAL_GetTick();
             state++;
    case  0: // select next starburst pixel and turn it full on
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 20;
             pixel_number = random(0, NUMBER_OF_LEDs-1);
             SetPixel(pixel_number, 0xff, 0xff, 0xff);
             Start_LEDs_tx();
             state++;
             break;
    case  1: // holding full on for 20 ms
             // wait for random delay before selecting next starburst pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += random(100,1000);
             SetPixel(pixel_number, SPARKLE_INTENSITY, SPARKLE_INTENSITY, SPARKLE_INTENSITY);
             Start_LEDs_tx();
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
uint8_t heat[NUMBER_OF_LEDs];

//------------------------------------------------------------------------------
void FireInit(void) {
  int16_t  i;

  for (i = 0; i < NUMBER_OF_LEDs; i++) {
    heat[i] = 0;
    }
}

//------------------------------------------------------------------------------
// return zero if completely cold, else return total heat
uint16_t FireCooling(uint32_t cooling) {
  uint32_t cooldown;
  uint16_t total_heat = 0;
  int16_t  i;

  // cool down every cell a little
  for (i = 0; i < NUMBER_OF_LEDs; i++) {
    cooldown = random(0, ((cooling * 10) / NUMBER_OF_LEDs) + 2);

    if (cooldown > heat[i]) heat[i]  = 0;
    else                    heat[i] -= cooldown;

    total_heat += heat[i];
    }

  return total_heat;
}

//------------------------------------------------------------------------------
void FireSparking(int32_t sparking) {
  int16_t i;

  // heat from each cell drifts up and diffuses a little
  for (i = NUMBER_OF_LEDs - 1; i >= 2; i--) {
    heat[i] = (heat[i - 1] + heat[i - 2] + heat[i - 2]) / 3;
    }

  // randomly ignite new sparks near the bottom
  if (random(0,255) < sparking) {
    heat[random(0,7)] += random(160,255);
    }
}

//------------------------------------------------------------------------------
void FireSetPixelHeatColor(void) {
  int16_t i;

  // convert heat to LED colors
  for (i = 0; i < NUMBER_OF_LEDs; i++) {
    // Scale 'heat' down from 0-255 to 0-191
    uint8_t t192 = (uint8_t) ((float) heat[i] / 255.0 * 191.0 + 0.5);

    // calculate ramp up from
    uint8_t heatramp = t192 & 0x3F;               // 0..63
    heatramp <<= 2;                               // scale up to 0..252

    // figure out which third of the spectrum we're in:
    if      (t192 > 0x80) SetPixel(NUMBER_OF_LEDs - 1 - i,      255,      255, heatramp);    // hottest
    else if (t192 > 0x40) SetPixel(NUMBER_OF_LEDs - 1 - i,      255, heatramp,        0);    // middle
    else                  SetPixel(NUMBER_OF_LEDs - 1 - i, heatramp,        0,        0);    // coolest
    }
}

//------------------------------------------------------------------------------
#define FIRE_COOLING      22                    // use 22 instead of 55 because we only have 24 LEDs instead of 60 LEDs
#define FIRE_SPARKING    120
#define FIRE_DELAY        15
#define FIRE_PAUSE      2000
void LightShow_Fire(bool init) {
  static  int16_t state = -1;
  static  int16_t fire_counter;
  static uint32_t target_tick;
  static uint16_t total_heat;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             FireInit();
             fire_counter = 150;
             target_tick  = HAL_GetTick();
             state++;
    case  0: // cooling and sparking
             FireCooling(FIRE_COOLING);
             FireSparking(FIRE_SPARKING);
             FireSetPixelHeatColor();
             Start_LEDs_tx();
             target_tick += FIRE_DELAY;
             state++;
             break;
    case  1: // wait for delay
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               // timer expired, check for done
               if (--fire_counter) state = 0;           // not done, keep going
               else                state = 2;           // done
               }
             break;
    case  2: // cool down completely
             total_heat = FireCooling(FIRE_COOLING/2);
             FireSetPixelHeatColor();
             Start_LEDs_tx();
             target_tick += FIRE_DELAY;
             state++;
             break;
    case  3: // wait for delay
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               // timer expired, check for cooldown
               if (total_heat) {
                 // not done, keep going
                 state = 2;
                 }
               else {
                 // done with cooldown, pause the light show effect
                 target_tick += FIRE_PAUSE;
                 state        = 4;
                 }
               }
             break;
    case  4: // wait for delay
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               // timer expired, restart the light show effect
               fire_counter = 150;
               state        = 0;
               }
             break;
    }
}

//------------------------------------------------------------------------------
typedef __packed struct {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
} pixel_t;

//------------------------------------------------------------------------------
pixel_t pixel[NUMBER_OF_LEDs];

//------------------------------------------------------------------------------
void fade_to_black(uint16_t pixel_number, uint8_t fade_value) {
  uint8_t r = pixel[pixel_number].red;
  uint8_t g = pixel[pixel_number].green;
  uint8_t b = pixel[pixel_number].blue;

  r = (r <= 10) ? 0 : (int32_t) r - (r * fade_value / 256);
  g = (g <= 10) ? 0 : (int32_t) g - (g * fade_value / 256);
  b = (b <= 10) ? 0 : (int32_t) b - (b * fade_value / 256);

  pixel[pixel_number].red   = r;
  pixel[pixel_number].green = g;
  pixel[pixel_number].blue  = b;
}

//------------------------------------------------------------------------------
#define METEOR_RAIN_INTENSITY   0xff
#define METEOR_SIZE               10
#define METEOR_TRAIL_DECAY        64
#define METEOR_RANDOM_DECAY        1
#define METEOR_SPEED_DELAY        30
#define METEOR_PAUSE            2000
void LightShow_MeteorRain(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int32_t pixel_sum;
  static  int16_t i;
          int16_t j;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             memset(pixel, 0, sizeof(pixel));
             target_tick = HAL_GetTick();
             i           = 0;
             state++;
    case  0: // fade brightness all LEDs one step
             for (j = 0; j < NUMBER_OF_LEDs; j++) {
               if ((!METEOR_RANDOM_DECAY) || (random(0,10) > 5)) {
                 fade_to_black(j, METEOR_TRAIL_DECAY);
                 }
               }
             // draw meteor
             for (j = 0; j < METEOR_SIZE; j++) {
               int16_t k = i - j;
               if ((k >= 0) && (k < NUMBER_OF_LEDs)) {
                 pixel[k].red   = METEOR_RAIN_INTENSITY;
                 pixel[k].green = 0;
                 pixel[k].blue  = METEOR_RAIN_INTENSITY;
                 }
               }
             // show pixels
             for (j = 0; j < NUMBER_OF_LEDs; j++) {
               SetPixel(j, pixel[j].red, pixel[j].green, pixel[j].blue);
               }
             Start_LEDs_tx();
             target_tick += METEOR_SPEED_DELAY;
             state++;
             break;
    case  1: // wait for delay before continuing
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               if (++i < (NUMBER_OF_LEDs + NUMBER_OF_LEDs)) state = 0;
               else                                         state = 2;
               }
             break;
    case  2: // wait for all pixels to fade
             pixel_sum = 0;
             for (j = 0; j < NUMBER_OF_LEDs; j++) {
               fade_to_black(j, METEOR_TRAIL_DECAY);
               SetPixel(j, pixel[j].red, pixel[j].green, pixel[j].blue);
               pixel_sum += pixel[j].red   ? 1 : 0;
               pixel_sum += pixel[j].green ? 1 : 0;
               pixel_sum += pixel[j].blue  ? 1 : 0;
               }
             Start_LEDs_tx();
             target_tick += METEOR_SPEED_DELAY;
             state++;
             break;
    case  3: // wait for delay before continuing
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               if (pixel_sum) state = 2;
               else {
                 // done with fade to black, pause the light show effect
                 target_tick += METEOR_PAUSE;
                 state = 4;
                 }
               }
             break;
    case  4: // wait for delay
             if ((int32_t) (target_tick - HAL_GetTick()) <= 0) {
               // timer expired, restart the light show effect
               state = -1;
               }
             break;
    }
}

//------------------------------------------------------------------------------
#define RUNNING_LIGHTS_DELAY   70
//const uint8_t HalfWave[6] = {0, 127, 220, 255, 220, 127};     // sinusoid half-wave
//const uint8_t HalfWave[6] = {0, 75, 197, 255, 197, 75};       // spreadsheet values from modeling the code
//const uint8_t HalfWave[6] = {0, 25, 197, 255, 197, 25};
//const uint8_t HalfWave[6] = {0, 25, 100, 255, 100, 25};
  const uint8_t HalfWave[6] = {0, 0, 50, 255, 50, 0};
void LightShow_RunningLights(bool init) {
  static  int16_t state = -1;
  static uint16_t phase;
  static uint32_t target_tick;
          int16_t i;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             phase       = 0;
             target_tick = HAL_GetTick();
             state++;
    case  0: // fill with half-wave rectified pattern
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += RUNNING_LIGHTS_DELAY;
             for (i = 0; i < NUMBER_OF_LEDs; i++) {
               int16_t j = (i + phase) % 6;                     // [index modulo 6]
               uint8_t RGB_value = HalfWave[j];
               SetPixel(i, RGB_value, RGB_value, RGB_value);
               }
             Start_LEDs_tx();
             phase++;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define BREATHE_DELAY   5
void LightShow_Breathe(bool init) {
  static  int16_t state = -1;
  static uint16_t intensity;
  static uint32_t target_tick;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             intensity   = 0;
             target_tick = HAL_GetTick();
             state++;
    case  0: // wait for fade-in  delay
    case  2: // wait for fade-out delay
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += BREATHE_DELAY;
             SetAllPixel(0, intensity, 0);
             Start_LEDs_tx();
             state++;
             break;
    case  1: // fade-in adjustment
             if (++intensity < 255) state = 0;  // continue fade-in
             else                   state = 2;  // start fade-out
             break;
    case  3: // fade-out adjustment
             if (--intensity > 0)   state = 2;  // continue fade-out
             else                   state = 0;  // start fade-in
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define CONNECT_DELAY   2000
void LightShow_Connect(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             state++;
    case  0: if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += CONNECT_DELAY;
             SetAllPixel(0, 0, 0xff);
             Start_LEDs_tx();
             state++;
             break;
    case  1: if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 4000;                       // setup dead time
             SetAllPixel(0, 0, 0);
             Start_LEDs_tx();
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define BLINK_DELAY   225
void LightShow_Blink(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t blink_count;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             blink_count = 3;
             state++;
    case  0: if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += BLINK_DELAY;
             SetAllPixel(0, 0xff, 0);
             Start_LEDs_tx();
             state++;
             break;
    case  1: if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += BLINK_DELAY;
             SetAllPixel(0, 0, 0);
             Start_LEDs_tx();
             if (--blink_count <= 0) {
               blink_count = 3;
               target_tick += 4000;                     // setup dead time
               }
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define SCROLL_UP_DELAY   70
void LightShow_ScrollUp(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             led_number  = NUMBER_OF_LEDs - 1;
             state++;
    case  0: // turn on the pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             SetPixel(led_number, 0x00, 0xff, 0x00);
             Start_LEDs_tx();
             target_tick += SCROLL_UP_DELAY;
             state++;
             break;
    case  1: // turn off the pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             SetPixel(led_number, 0x00, 0x00, 0x00);
             if (--led_number < 0) {
               led_number = NUMBER_OF_LEDs - 1;
               target_tick += 4000;                     // setup dead time
               Start_LEDs_tx();
               }
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define SCROLL_DOWN_DELAY   70
void LightShow_ScrollDown(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             led_number  = 0;
             state++;
    case  0: // turn on the pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             SetPixel(led_number, 0x00, 0xff, 0x00);
             Start_LEDs_tx();
             target_tick += SCROLL_DOWN_DELAY;
             state++;
             break;
    case  1: // turn off the pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             SetPixel(led_number, 0x00, 0x00, 0x00);
             if (++led_number >= NUMBER_OF_LEDs) {
               led_number = 0;
               target_tick += 4000;                     // setup dead time
               Start_LEDs_tx();
               }
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define FILL_FULL_DELAY   70
#define FILL_FULL_HOLD  1000
void LightShow_FillFull(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             led_number  = NUMBER_OF_LEDs - 1;
             state++;
    case  0: // turn on the next pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += FILL_FULL_DELAY;
             SetPixel(led_number, 0x00, 0xff, 0x00);
             Start_LEDs_tx();
             if (--led_number < 0) {
               led_number   = NUMBER_OF_LEDs - 1;
               target_tick += FILL_FULL_HOLD;
               state++;
               }
             break;
    case  1: // hold 
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 1000;                       // setup dead time
             SetAllPixel(0, 0, 0);
             Start_LEDs_tx();
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define FILL_HALF_DELAY   70
#define FILL_HALF_HOLD  1000
void LightShow_FillHalf(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             target_tick = HAL_GetTick();
             led_number  = NUMBER_OF_LEDs - 1;
             state++;
    case  0: // turn on the next pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += FILL_HALF_DELAY;
             SetPixel(led_number, 0x00, 0xff, 0x00);
             Start_LEDs_tx();
             if (--led_number < NUMBER_OF_LEDs/2) {
               led_number  = NUMBER_OF_LEDs - 1;
               target_tick += FILL_HALF_HOLD;
               state++;
               }
             break;
    case  1: // hold
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += 1000;                       // setup dead time
             SetAllPixel(0, 0, 0);
             Start_LEDs_tx();
             state = 0;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define EMPTY_FULL_DELAY   70
#define EMPTY_FULL_HOLD   400
void LightShow_EmptyFull(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             SetAllPixel(0x00, 0xff, 0x00);
             Start_LEDs_tx();
             target_tick = HAL_GetTick() + EMPTY_FULL_HOLD;
             led_number  = 0;
             state++;
    case  0: // turn off the next pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += EMPTY_FULL_DELAY;
             SetPixel(led_number, 0x00, 0x00, 0x00);
             Start_LEDs_tx();
             if (++led_number >= NUMBER_OF_LEDs) {
               target_tick += 2000;                     // setup dead time
               state++;
               }
             break;
    case  1: // hold
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             state = -1;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
#define EMPTY_HALF_DELAY   70
#define EMPTY_HALF_HOLD   400
void LightShow_EmptyHalf(bool init) {
  static  int16_t state = -1;
  static uint32_t target_tick;
  static  int16_t led_number;

  if (init) state = -1;
  switch (state) {
    case -1: // init
             for (led_number = NUMBER_OF_LEDs/2; led_number < NUMBER_OF_LEDs; led_number++)
               SetPixel(led_number, 0x00, 0xff, 0x00);
             Start_LEDs_tx();
             target_tick = HAL_GetTick() + EMPTY_HALF_HOLD;
             led_number  = NUMBER_OF_LEDs/2;
             state++;
    case  0: // turn off the next pixel
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             target_tick += EMPTY_HALF_DELAY;
             SetPixel(led_number, 0x00, 0x00, 0x00);
             Start_LEDs_tx();
             if (++led_number >= NUMBER_OF_LEDs) {
               target_tick += 2000;                     // setup dead time
               state++;
               }
             break;
    case  1: // hold
             if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             state = -1;
             break;
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// RGB value is 0..255
void LightShow_RGB(bool init, uint16_t R, uint16_t G, uint16_t B) {
  if (init) {
    SetAllPixel(R, G, B);
    Start_LEDs_tx();
    }
}

//------------------------------------------------------------------------------
void LightShow_Red  (bool init) {LightShow_RGB(init, 255,   0,   0);}
void LightShow_Green(bool init) {LightShow_RGB(init,   0, 255,   0);}
void LightShow_Blue (bool init) {LightShow_RGB(init,   0,   0, 255);}
void LightShow_White(bool init) {LightShow_RGB(init, 255, 255, 255);}
void LightShow_Off  (bool init) {LightShow_RGB(init,   0,   0,   0);}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// 0=non-ball crossing 1=ball crossing
// for ball crossing show white background followed by a purple smear
// for non-ball crossing show orange background followed by a white smear
void LightShow_Crossing(bool init, uint16_t red2a, uint16_t green2a, uint16_t blue2a,
                                   uint16_t red2b, uint16_t green2b, uint16_t blue2b) {
  static  int16_t state = -1;
  static uint32_t LED_timer;
         uint32_t mask;
         uint32_t LEDs2a;       // layer 2a LEDs
         uint32_t LEDs2b;       // layer 2b LEDs
          int16_t i;

  if (init) state = -1;
  switch (state) {
    case -1: // show all LEDs in RGB2a
             if ((red2a & green2a & blue2a) == 255) SetAllPixel(red2a, green2a, blue2a);
             else {
               mask   = (uint32_t) 1 << 31;
               LEDs2a = LightShowRequestMsg.LEDs2;

               for (i = 0; i < NUMBER_OF_LEDs; i++) {
                 if (LEDs2a & mask) SetPixel(i, red2a, green2a, blue2a);
                 else               SetPixel(i,     0,       0,      0);
                 mask >>= 1;
                 }
               }

             Start_LEDs_tx();
             LED_timer = HAL_GetTick() + LightShowRequestMsg.ball_crossing_time_ms;
             state++;
    case  0: // show layer 2a for 300 ms
             if ((int32_t) (LED_timer - HAL_GetTick()) > 0) break;
             state++;
    case  1: // show layer 2b (the smear) for 600 ms
             mask   = (uint32_t) 1 << 31;
             LEDs2b =  LightShowRequestMsg.LEDs2;

             for (i = 0; i < NUMBER_OF_LEDs; i++) {
               if (LEDs2b & mask) SetPixel(i, red2b, green2b, blue2b);
               else               SetPixel(i,     0,       0,      0);
               mask >>= 1;
               }

             Start_LEDs_tx();
             LED_timer = HAL_GetTick() + 600;
             state++;
    case  2: // show smear for 600 ms
             if ((int32_t) (LED_timer - HAL_GetTick()) > 0) break;
             SetAllPixel(0, 0, 0);
             Start_LEDs_tx();
             state++;
    case  3: break;
    }
}

//------------------------------------------------------------------------------
void LightShow_Non_Ball_Crossing(bool init) {LightShow_Crossing(init, 128,  50,   0,    // rgb2a orange
                                                                      128,  50,   0);}  // rgb2b orange
void LightShow_Ball_Crossing    (bool init) {LightShow_Crossing(init, 255, 255, 255,    // rgb2a white
                                                                      255, 255, 255);}  // rgb2a white

//------------------------------------------------------------------------------
#define LIGHTSHOW_NUM_ENTRIES (sizeof(LightShowList) / sizeof(LightShowList[0]))
void (*LightShowList[])(bool) = {
  LightShow_Off,                        //  0
  LightShow_ColorWipe,                  //  1
  LightShow_FadeInandFadeOutRGB,        //  2
  LightShow_Sparkle,                    //  3
  LightShow_Fire,                       //  4
  LightShow_MeteorRain,                 //  5
  LightShow_RunningLights,              //  6
  LightShow_Breathe,                    //  7
  LightShow_Connect,                    //  8
  LightShow_Blink,                      //  9
  LightShow_ScrollUp,                   // 10
  LightShow_ScrollDown,                 // 11
  LightShow_FillFull,                   // 12
  LightShow_FillHalf,                   // 13
  LightShow_EmptyFull,                  // 14
  LightShow_EmptyHalf,                  // 15
  LightShow_Red,                        // 16
  LightShow_Green,                      // 17
  LightShow_Blue,                       // 18
  LightShow_White,                      // 19
  LightShow_Non_Ball_Crossing,          // 20
  LightShow_Ball_Crossing               // 21
};

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// bit 0 is bottom-most LED
uint32_t LEDs_red   = 0x3f;
uint32_t LEDs_green = 0x3f;
uint32_t LEDs_blue  = 0x3f;
uint16_t POST_state = 0;

uint16_t LightShow_POST_update_request  = 0;
uint16_t LightShow_POST_update_complete = 0;

void LightShow_POST(void) {
  uint32_t mask = 1ul << 31;
  int16_t  i;

  // check for requests
  if ((int16_t) (LightShow_POST_update_request - LightShow_POST_update_complete) <= 0) return;
  LightShow_POST_update_complete++;

  // pixel 0 is top-most LED
  for (i = 0; i < NUMBER_OF_LEDs; i++) {
    SetPixel(i, (LEDs_red   & mask) ? 255 : 0,
                (LEDs_green & mask) ? 255 : 0,
                (LEDs_blue  & mask) ? 255 : 0);
    mask >>= 1;
    }

  // output the LEDs
  Start_LEDs_tx();
}

//------------------------------------------------------------------------------
int16_t LightShow_Selector = 0;
bool    LightShow_Init     = true;
void LightShowTask(void) {
  static int16_t  state = -1;
  static uint32_t LED_timer;

  // assure previous LED tx is complete before starting another
  if (LED_tx_busy) return;

  switch (state) {
    case -1: LED_Data_Init();
             // delay for LED power supply
             LED_timer = HAL_GetTick() + 100;
             state++;
    case  0: if ((int32_t) (LED_timer - HAL_GetTick()) > 0) break;
             LED_timer = HAL_GetTick() + 100;
             LightShow_POST_update_request++;
             LightShow_POST();
             state++;
    case  1: // wait for serial packet from opposite side
             if (code_image_crc_msg_count == 0) break;
             state++;
    case  2: // calibrating emitter PWMs and running POST
             // check for POST complete
             LightShow_POST();
             if (POST_state >= 2) {
               // power on self test finished,
               // run optional light show effect
               LightShow_Selector = LightShow_Selector_test;
               state++;
               }
             break;
    case  3: // POST complete, normal LED effects
             if (LightShow_Selector < 0) {
               LightShow_RGB_Indicator_control_task();
               }
             else {
               if (LightShow_Selector >= LIGHTSHOW_NUM_ENTRIES) 
                   LightShow_Selector  = e_LightShow_Off;

               if (LightShow_Init) memset(LightShowData, LED_zero_bit, NUMBER_OF_LED_BYTES);
               (*LightShowList[LightShow_Selector])(LightShow_Init);
               LightShow_Init = false;
               }
             break;
    }
}
