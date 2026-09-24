// LEDs.c
//
// very basic code to turn on all LEDs to a specified color

#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "LEDs.h"

//------------------------------------------------------------------------------
extern SPI_HandleTypeDef hspi3;
#define LED_SPI          hspi3

//------------------------------------------------------------------------------
// forward references
// void SetPixel(uint16_t index, uint16_t red, uint16_t green, uint16_t blue);
// void SetAllPixel(uint16_t red, uint16_t green, uint16_t blue);

//------------------------------------------------------------------------------
// Light Show LED driver for Inolux IN-PI42TAS(X)R(X)G(X)B
// use SPI to output the one-bits and zero-bits
//
// SPI clock is 5.625 MBits / second
// one SPI byte represents the timing required for a one-bit or a zero-bit to the LEDs
// each LED is 24-bits (Green | Red | Blue), msb first (not RGB!)
// there are 24 LEDs in a string
// so 24*(GRB) = 24 * 24 = 576 bytes
// followed by 80 us of SPI output low to reset
// 
// the timing for a zero-bit is (0.2 us <= T0H <= 0.4)
// followed by                  (0.8 us <= T0L)
//
// the timing for a zero-bit is (0.62 us <= T1H <= 1.0)
// followed by                  (0.20 us <= T1L)
//
// the timing for a complete zero-bit or one-bit: 1.25us <= (TH + TL)
//
// the part adopts the new value after reset (80 us <= Trst)
//
// the SPI wants to leave the output pin equal to the MSB of the last data byte
// so we want to me sure the MSB is zero to ensure we do not confuse the T0H and T1H timing
// this is easily done by shifting the pattern right so that there is a zero in the MSB
//
// use 0x7c for  one-bit timing
// use 0x60 for zero-bit timing
//
// possible sequences:
// 0x60 followed by 0x60:       0110000001100000        T0H=2/5.625 = 0.355     T0L=6/5.625 = 1.066
// 0x60 followed by 0x7c:       0110000001111100        T0H=2/5.625 = 0.355     T0L=6/5.625 = 1.066
// 0x7c followed by 0x60:       0111110001100000        T1H=5/5.625 = 0.888     T1L=3/5.625 = 0.533
// 0x7c followed by 0x7c:       0111110001111100        T1H=5/5.625 = 0.888     T1L=3/5.625 = 0.533
// 
// the shift order is unexpected:
// the first  GRB value shifted out of the SPI is accepted by the first  LED, and its DO pin is low for the first  GRB value
// the second GRB value shifted out of the SPI is accepted by the second LED, and its DO pin is low for the second GRB value
// ...
//
// at 5.625 MBits / second, one byte is 8 bits * (1 us / 5.625 bits) = 1.42222 us
// 24 bytes per LED
// 80 us of reset requires 56.25 bytes of output low
// (2 + 56*8) = 450 bits * (1 us / 5.625 bits) = 80 us                          <-- the "2" comes from: the last two bits of a one-bit or a zero-bit are always 00
//
#define LED_zero_bit          0x60
#define LED_one_bit           0x7c

#define NUMBER_OF_LEDs          32                                              // 32 LEDs
#define NUMBER_OF_LED_BYTES     (NUMBER_OF_LEDs*24)                             // 24 bytes per LED
#define NUMBER_OF_RESET_BYTES   56

#define NUMBER_OF_STATUS_LEDs           4                                       // 32 LEDs
#define NUMBER_OF_STATUS_LED_BYTES      (NUMBER_OF_STATUS_LEDs*24)              // 24 bytes per LED
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

//------------------------------------------------------------------------------
// index 0 is the top of the LED stack, index 31 is the bottom
void SetPixel(uint16_t index, uint16_t red, uint16_t green, uint16_t blue) {
  uint8_t *GRB = LightShowData + index*24;

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
// index 0 is top, index 31 is bottom
void SetAllPixel(uint16_t red, uint16_t green, uint16_t blue) {
  int16_t i;

  for (i = 0; i < NUMBER_OF_LEDs; i++) SetPixel(i, red, green, blue);
}

//------------------------------------------------------------------------------
volatile bool LED_tx_busy = false;

//------------------------------------------------------------------------------
// start new LED DMA transfer
void Start_LEDs_tx(void) {
  if (LED_tx_busy) return;

  LED_tx_busy = true;
  HAL_SPI_Transmit_DMA(&LED_SPI, LightShowData, sizeof(LightShowData));
}

//------------------------------------------------------------------------------
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
  if (hspi == &LED_SPI) LED_tx_busy = false;
}

//------------------------------------------------------------------------------
void Bootloader_LED_task(uint16_t show) {
  static int16_t  state = 0;
  static uint32_t target_tick;

  switch (state) {
    case  0: // set all pixels to red
             target_tick = HAL_GetTick() + 50;
             SetAllPixel(255, 0, 0);
             state++;
    case  1: if ((int32_t) (target_tick - HAL_GetTick()) > 0) break;
             if (LED_tx_busy)                                 break;
             LED_tx_busy = true;
             HAL_SPI_Transmit_DMA(&LED_SPI, LightShowData, sizeof(LightShowData));
             state++;
    case  2: if (show) break;
             while (LED_tx_busy) {}
             SetAllPixel(0, 0, 0);
             HAL_SPI_Transmit_DMA(&LED_SPI, LightShowData, sizeof(LightShowData));
             state++;
    case  3: break;
    }
}

void Show_Bootloader_LEDs(void) {Bootloader_LED_task(1);}

void Hide_Bootloader_LEDs(void) {Bootloader_LED_task(0);}
