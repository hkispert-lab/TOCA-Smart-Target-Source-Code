//------------------------------------------------------------------------------
// zero bit detection

#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "Zero_bit.h"

//------------------------------------------------------------------------------
// find the left-most zero bit,
// which corresponds to the top of the light curtain
// used for Measured_shadow_high[]
// for 0xffffffff index = 32
// for 0x7fffffff index = 31
// for 0xfffffffe index =  0
int16_t Find_Most_Significant_zero_bit(uint32_t vector) {
  int16_t i = 32;

  if (vector != 0xfffffffful) {
    while ((--i > 0) && (vector & 0x80000000ul)) {
      vector <<= 1;
      }
    }

  return i;
}

//------------------------------------------------------------------------------
// find the right-most zero bit,
// which corresponds to the bottom of the light curtain
// used for Measured_shadow_low[]
// for 0x7fffffff index = 31
// for 0xfffffffe index =  0
// for 0xffffffff index = -1
int16_t Find_Least_Significant_zero_bit(uint32_t vector) {
  int16_t i = -1;

  if (vector != 0xfffffffful) {
    while ((++i < 32) && (vector & 1ul)) {
      vector >>= 1;
      }
    }

  return i;
}

//------------------------------------------------------------------------------
const uint32_t vector_left_zero_data[];
const uint32_t vector_right_zero_data[];
void Find_zero_bit_test(void) {
  int16_t i;

  for (i = 0; i <= 32; i++) {
    uint32_t vector = vector_left_zero_data[i];
    if (Find_Most_Significant_zero_bit(vector) != (32-i))
      __NOP();

    vector = vector_right_zero_data[i];
    if (Find_Least_Significant_zero_bit(vector) != (i-1))
      __NOP();
    }
}

//------------------------------------------------------------------------------
const uint32_t vector_left_zero_data[] = {
  0xfffffffful,         // 32
  0x7ffffffful,         // 31
  0xbffffffful,         // 30
  0xdffffffful,         // 29
  0xeffffffful,         // 28
  0xf7fffffful,         // 27
  0xfbfffffful,         // 26
  0xfdfffffful,         // 25
  0xfefffffful,         // 24
  0xff7ffffful,         // 23
  0xffbffffful,         // 22
  0xffdffffful,         // 21
  0xffeffffful,         // 20
  0xfff7fffful,         // 19
  0xfffbfffful,         // 18
  0xfffdfffful,         // 17
  0xfffefffful,         // 16
  0xffff7ffful,         // 15
  0xffffbffful,         // 14
  0xffffdffful,         // 13
  0xffffeffful,         // 12
  0xfffff7fful,         // 11
  0xfffffbfful,         // 10
  0xfffffdfful,         //  9
  0xfffffefful,         //  8
  0xffffff7ful,         //  7
  0xffffffbful,         //  6
  0xffffffdful,         //  5
  0xffffffeful,         //  4
  0xfffffff7ul,         //  3
  0xfffffffbul,         //  2
  0xfffffffdul,         //  1
  0xfffffffeul          //  0
};

const uint32_t vector_right_zero_data[] = {
  0xfffffffful,         // -1
  0xfffffffeul,         //  0
  0xfffffffdul,         //  1
  0xfffffffbul,         //  2
  0xfffffff7ul,         //  3
  0xffffffeful,         //  4
  0xffffffdful,         //  5
  0xffffffbful,         //  6
  0xffffff7ful,         //  7
  0xfffffefful,         //  8
  0xfffffdfful,         //  9
  0xfffffbfful,         // 10
  0xfffff7fful,         // 11
  0xffffeffful,         // 12
  0xffffdffful,         // 13
  0xffffbffful,         // 14
  0xffff7ffful,         // 15
  0xfffefffful,         // 16
  0xfffdfffful,         // 17
  0xfffbfffful,         // 18
  0xfff7fffful,         // 19
  0xffeffffful,         // 20
  0xffdffffful,         // 21
  0xffbffffful,         // 22
  0xff7ffffful,         // 23
  0xfefffffful,         // 24
  0xfdfffffful,         // 25
  0xfbfffffful,         // 26
  0xf7fffffful,         // 27
  0xeffffffful,         // 28
  0xdffffffful,         // 29
  0xbffffffful,         // 30
  0x7ffffffful,         // 31
};
