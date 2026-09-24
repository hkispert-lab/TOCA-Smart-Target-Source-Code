#ifndef __LEDSCOMMON_H
#define __LEDSCOMMON_H

//------------------------------------------------------------------------------
// Light Show LED driver for Inolux IN-PI42TAS(X)R(X)G(X)B
// use SPI to output the one-bits and zero-bits
//
// SPI clock is 5.625 MBits / second
// one SPI byte represents the timing required for a one-bit or a zero-bit to the LEDs
// each LED is 24-bits (Green | Red | Blue), msb first (not RGB!)
// there are 32 LEDs in a string
// so 32*(GRB) = 32 * 24 = 768 bytes
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
#define LED_zero_bit                    0x60
#define LED_one_bit                     0x7c

#define NUMBER_OF_LEDs                  32                                      // 32 LEDs
#define NUMBER_OF_LED_BYTES             (NUMBER_OF_LEDs*24)                     // 24 bytes per LED

#define NUMBER_OF_STATUS_LEDs           4                                       //  4 LEDs
#define NUMBER_OF_STATUS_LED_BYTES      (NUMBER_OF_STATUS_LEDs*24)              // 24 bytes per LED

#define NUMBER_OF_RESET_BYTES           56

enum {
  e_LightShow_Off,                      //  0
  e_LightShow_ColorWipe,                //  1
  e_LightShow_FadeInandFadeOutRGB,      //  2
  e_LightShow_Sparkle,                  //  3
  e_LightShow_Fire,                     //  4
  e_LightShow_MeteorRain,               //  5
  e_LightShow_RunningLights,            //  6
  e_LightShow_Breathe,                  //  7
  e_LightShow_Connect,                  //  8
  e_LightShow_Blink,                    //  9
  e_LightShow_ScrollUp,                 // 10
  e_LightShow_ScrollDown,               // 11
  e_LightShow_FillFull,                 // 12
  e_LightShow_FillHalf,                 // 13
  e_LightShow_EmptyFull,                // 14
  e_LightShow_EmptyHalf,                // 15
  e_LightShow_Red,                      // 16
  e_LightShow_Green,                    // 17
  e_LightShow_Blue,                     // 18
  e_LightShow_White,                    // 19
  e_LightShow_Non_Ball_Crossing,        // 20
  e_LightShow_Ball_Crossing             // 21
};

extern int16_t LightShow_Selector_test;

extern uint8_t LightShowData  [NUMBER_OF_LED_BYTES        + NUMBER_OF_RESET_BYTES];    // (num LEDs) * (3*8 bytes for GRB) + (56 output low bytes for reset)
extern uint8_t StatusLightData[NUMBER_OF_STATUS_LED_BYTES + NUMBER_OF_RESET_BYTES];    // (num LEDs) * (3*8 bytes for GRB) + (56 output low bytes for reset)

void SetStatusLED(uint16_t index, uint16_t red, uint16_t green, uint16_t blue);
void SetPixel    (uint16_t index, uint16_t red, uint16_t green, uint16_t blue);

#endif
