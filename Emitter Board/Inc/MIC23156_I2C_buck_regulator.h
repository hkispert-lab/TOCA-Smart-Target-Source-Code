#ifndef __MIC23156_H
#define __MIC23156_H

#define EmitterVoltage_min_mv            700            //  700 mV is min
#define EmitterVoltage_max_mv           2400            // 2400 mV is max
//#define EmitterVoltage_default_mv     1400            // 1400 mV for 12 inch frame
  #define EmitterVoltage_default_mv     2400            // 2400 mV for 42 inch frame

extern uint16_t EmitterVoltage_mv;

void Set_emitter_voltage(uint16_t V_mv);

#endif
