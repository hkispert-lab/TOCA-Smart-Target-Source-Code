// MIC23156 Sync Buck Regulator
// 

//#include <string.h>
//#include <stdlib.h>
//#include <stdint.h>
//#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "MIC23156_I2C_buck_regulator.h"

//------------------------------------------------------------------------------
// I2C setup:
//
// Mode: I2C
// Parameter settings:
//      I2C speed mote: standard mode
//      I2C clock speed (Hz) 100000
//      clock no stretch mode disabled
//      primary address length selection 7-bit
//      dual address acknowledged disabled
//      primary slave address 0
//      general call address detection disabled
// User constants none
// NVIC Interrupts none
// DMA Settings none
// GPIO Settings:
//      PB6 I2C1_SCL    Alternate, pullup, very high
//      PB7 I2C1_SDA    Alternate, pullup, very high

//------------------------------------------------------------------------------
extern  I2C_HandleTypeDef          hi2c1;
#define MIC23156_I2C               hi2c1
#define MIC23156_I2C_SLAVE_ADDR    0xb6
#define MIC23156_enable_status_reg 0x01
#define MIC23156_buck_out1_reg     0x02
#define MIC23156_buck_out2_reg     0x03

uint8_t write_buck_out1[] = {MIC23156_buck_out1_reg, 0x00};     // reg num=2, BUCK_OUT1
uint8_t write_buck_out2[] = {MIC23156_buck_out2_reg, 0x00};     // reg num=3, BUCK_OUT2
uint16_t EmitterVoltage_mv = EmitterVoltage_default_mv;         // 2.400V

//------------------------------------------------------------------------------
// set the emitter power supply voltage
//     0 counts =  700 mv
//   170 counts = 2400 mv
// > 170 counts = 2400 mv
//
// x counts = ((desired V in mv) - 700)/10
//
// so, for a desired voltage of 2 V:
//
// x counts = (2000 - 700) / 10
void Set_emitter_voltage(uint16_t V_mv) {
  uint8_t counts;

  // range check the desired voltage
  if (V_mv < EmitterVoltage_min_mv) V_mv = EmitterVoltage_min_mv;       //  700 mv
  if (V_mv > EmitterVoltage_max_mv) V_mv = EmitterVoltage_max_mv;       // 2400 mv

  counts = (V_mv - EmitterVoltage_min_mv) / 10;
  write_buck_out1[1] = 0;
  write_buck_out2[1] = counts;

  // minimum 0.7V   is on Vsel=0 (selected by unhappy emitter watchdog)
  // the set output is on Vsel=1 (selected by   happy emitter watchdog)
  HAL_I2C_Master_Transmit(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, write_buck_out1, sizeof(write_buck_out1), 200);
  HAL_I2C_Master_Transmit(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, write_buck_out2, sizeof(write_buck_out2), 200);
}

//------------------------------------------------------------------------------
#if 0
uint8_t write_enable_status[] = {MIC23156_enable_status_reg, 0x00};    // reg num=1, |0|TSD|UVLO|PGOOD|0|0|SSL|BUCK_EN|

uint8_t read_enable_status;
uint8_t read_buck_out1;
uint8_t read_buck_out2;


  //i2c1_write_enable_status[1] ^= 1;

  //HAL_I2C_Master_Transmit(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, write_enable_status, sizeof(write_enable_status), 200);
    HAL_I2C_Master_Transmit(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, write_buck_out1,     sizeof(write_buck_out1),     200);
    HAL_I2C_Master_Transmit(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, write_buck_out2,     sizeof(write_buck_out2),     200);

    HAL_I2C_Mem_Read(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, MIC23156_enable_status_reg, 1, &read_enable_status, sizeof(read_enable_status), 200);
    HAL_I2C_Mem_Read(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, MIC23156_buck_out1_reg    , 1, &read_buck_out1,     sizeof(read_buck_out1),     200);
    HAL_I2C_Mem_Read(&MIC23156_I2C, MIC23156_I2C_SLAVE_ADDR, MIC23156_buck_out2_reg    , 1, &read_buck_out2,     sizeof(read_buck_out2),     200);
#endif
