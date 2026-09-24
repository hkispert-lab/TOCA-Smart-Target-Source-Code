#include <stdint.h>
#include "main.h"
#include "UART.h"
#include "MIC23156_I2C_buck_regulator.h"
#include "LEDs.h"
#include "LightCurtain.h"

//------------------------------------------------------------------------------
// module handles
extern ADC_HandleTypeDef hadc1;
extern ADC_HandleTypeDef hadc2;
extern TIM_HandleTypeDef htim8;
extern DMA_HandleTypeDef hdma_tim8_up;

//------------------------------------------------------------------------------
// the emitter sequence used is described in file "Emitter detector sequence.xlsx"
//
// const uint16_t Emitter_Select_Codes[EMITTER_SIZE] = {
//    0,  17,  2, 19,  4, 21,  6, 23,  8, 25, 10, 27, 12, 29, 14, 31,
//    16,  1, 18,  3, 20,  5, 22,  7, 24,  9, 26, 11, 28, 13, 30, 15
// };

// ADC results data and unscrambled ADC data and filtered ADC data
uint16_t Detector_ADC1_Results[EMITTER_SIZE];   // unfiltered emitter ADC1 results for a complete scan        (scrambled   order)
uint16_t Detector_ADC2_Results[EMITTER_SIZE];   // unfiltered emitter ADC2 results for a complete scan        (scrambled   order)
uint16_t Emitter_Light_ADC    [EMITTER_SIZE];   // unfiltered emitter ADC  results for a complete Light scan  (unscrambled order)
uint32_t Emitter_Light_ADC_Q10[EMITTER_SIZE];   //   filtered emitter ADC  results for a complete Light scan  (unscrambled order)
uint32_t Emitter_Vector = ~0ul;                 // bitvector containing 1=emitter/detector not blocked 0=blocked
                                                // emitter 0/detector 0 (in hardware) is the top-most emitter/detector pair
                                                // which corresponds to bit 31 in the bit vector
                                                // emitter 31/detector 15 (in hardware) is the bottom-most emitter/detector pair
                                                // which corresponds to bit 0 in the bit vector

// ADC result FIFO counters
uint16_t Detector_ADC1_Results_HalfCplt_Cnt_ISR = 0; uint16_t Detector_ADC1_Results_HalfCplt_Cnt_Task = 0;
uint16_t Detector_ADC2_Results_Cplt_Cnt_ISR     = 0; uint16_t Detector_ADC2_Results_Cplt_Cnt_Task     = 0;

//------------------------------------------------------------------------------
// forward references
void Emitters_DMA_HalfCpltCallback(DMA_HandleTypeDef *hdma);
void Emitters_DMA_CpltCallback    (DMA_HandleTypeDef *hdma);
void HAL_GPIO_PDC_TogglePin       (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

//------------------------------------------------------------------------------
// emitter light reading sample is complete
// find max light reading and adjust voltage so that we are on desired max reading
// update twice per second

uint16_t EmitterSetPWMsMsg_pct[32];
uint16_t EmitterSetPWMs_max_ADC     = 1500;
uint16_t EmitterSetPWMs_min_ADC     = 1200;
uint32_t EmitterSetPWMs_interval_ms =  500;

#define minimum_threshold             1000      // filtered light reading must be at least 1000 to be usable
#define blocked_threshold              500      // unfiltered blocked reading must be less than 500 to be considered "blocked"
#define recovery_threshold            1000      // unfiltered reading must be at least 1000 to be considered "no longer blocked"

//------------------------------------------------------------------------------
bool Emitter_power_adjust(void) {
   int16_t slot;
  uint16_t emitter;
  uint16_t adc;
   int16_t pct;
  bool changed = false;

  for (slot = 0; slot < NUM_SLOTS; slot++) {
    emitter = EmitterSetPWMsMsg.Emitter_slot_rec[slot].emitter;
    pct     = EmitterSetPWMsMsg.Emitter_slot_rec[slot].pct;
    adc     = Emitter_Light_ADC[emitter];
  
    if      (adc < EmitterSetPWMs_min_ADC) {if (++pct > 100) pct = 100; else changed = true;}
    else if (adc > EmitterSetPWMs_max_ADC) {if (--pct <   0) pct =   0; else changed = true;}

    EmitterSetPWMsMsg.Emitter_slot_rec[slot].pct = pct;
    EmitterSetPWMsMsg_pct[emitter]               = pct;
    }

  return changed;
}

//------------------------------------------------------------------------------
// LEDs[31] is top LED, each bit: 0=out of calibration 1=cal is OK
// Emitter_Light_ADC[0] is top emitter
uint32_t Emitter_cal_check(void) {
  uint32_t cal = 0;                             // init: all emitter cal is NG
  uint32_t mask = 1ul << 31;
   int16_t emitter;

  // check unscrambled emitter light reading for good calibration
  for (emitter = 0; emitter < EMITTER_SIZE; emitter++) {
    uint16_t adc = Emitter_Light_ADC[emitter];

    // check the cal for this emitter
    if ((adc >= EmitterSetPWMs_min_ADC) && (adc <= EmitterSetPWMs_max_ADC)) {
      cal |= mask;                              // emitter cal is OK
      }

    mask >>= 1;
    }

  return cal;
}

//------------------------------------------------------------------------------
// turn on all emitters (restore PWM settings)
void Emitter_all_on(void) {
   int16_t slot;
  uint16_t emitter;

  for (slot = 0; slot < NUM_SLOTS; slot++) {
    emitter = EmitterSetPWMsMsg.Emitter_slot_rec[slot].emitter;
    EmitterSetPWMsMsg.Emitter_slot_rec[slot].pct = EmitterSetPWMsMsg_pct[emitter];
    }
}

//------------------------------------------------------------------------------
void Emitter_power_off(uint16_t emitter) {
   int16_t slot;

#if 0
  // for testing: ignore emitters to generate a fault
  if ((emitter ==  5) ||
      (emitter ==  6) ||
      (emitter == 25)) return;
#endif

  // turn on all emitters (restore PWM settings)
  Emitter_all_on();

  for (slot = 0; slot < NUM_SLOTS; slot++) {
    if (EmitterSetPWMsMsg.Emitter_slot_rec[slot].emitter == emitter) {
      EmitterSetPWMsMsg.Emitter_slot_rec[slot].pct = 0;
      break;
      }
    }
}

//------------------------------------------------------------------------------
bool Emitter_off_check(uint16_t emitter) {
  return Emitter_Light_ADC[emitter] < blocked_threshold;
}

//------------------------------------------------------------------------------
uint32_t calibration_outlier_fault = 0;         // 1-bits are outlier errors
uint32_t Emitter_LEDs_red;
uint32_t Emitter_LEDs_green;
uint32_t Emitter_LEDs_blue;

void Emitter_Power_Task(bool normal_adc_sequence) {
  static uint32_t PWM_timer;
  static uint32_t LED_timer;
  static uint32_t mask;
  static uint32_t cal_check_result;
  static  int16_t state             = -1;
  static uint16_t packet_send_count =  0;
  static uint16_t packet_sent_count =  0;
  static uint16_t emitter;
         bool     PWMs_changed = false;
         bool     LEDs_changed = false;
          int16_t i;

  switch (state) {
    case -2: // POST error, stay in this state
             break;
    case -1: // init, force sending first packet
             PWM_timer  = HAL_GetTick();
             LED_timer  = HAL_GetTick();
             POST_state = 1;                            // POST is running
             state++;
    case  0: // POST initial calibration step

             // PWM calibration adjustment every 500 ms
             if ((int32_t) (PWM_timer - HAL_GetTick()) <= 0) {
               PWM_timer += EmitterSetPWMs_interval_ms;

               // make PWM adjustment and look for calibration complete
               PWMs_changed = Emitter_power_adjust();
               if (PWMs_changed == false) {
                 // PWM calibration complete
                 // capture the cal check now before ADCs change
                 cal_check_result = Emitter_cal_check();

                 // show all white both sides for 1 second
                 LEDs_red           = ~0ul;
                 LEDs_green         = ~0ul;
                 LEDs_blue          = ~0ul;
                 Emitter_LEDs_red   = ~0ul;
                 Emitter_LEDs_green = ~0ul;
                 Emitter_LEDs_blue  = ~0ul;
                 LEDs_changed       = true;
                 PWM_timer         += 1000;

                 state++;
                 }
               }

             // run a circular pattern of white LEDs up the vertical LEDs every 100 ms
             // while emitter PWMs are calibrating
             if ((int32_t) (LED_timer - HAL_GetTick()) <= 0) {
               LED_timer += 100;

               mask               = (LEDs_red & (1ul << 31)) ? 1 : 0;
               LEDs_red           = (LEDs_red << 1) | mask;
               LEDs_green         =  LEDs_red;
               LEDs_blue          =  LEDs_red;
               Emitter_LEDs_red   =  LEDs_red;
               Emitter_LEDs_green =  LEDs_red;
               Emitter_LEDs_blue  =  LEDs_red;
               LEDs_changed       =  true;
               LightShow_POST_update_request++;
               }

             if (PWMs_changed || LEDs_changed) packet_send_count++;
             break;
    case  1: // PWM calibration complete
             // show all white both sides for 1 second
             if ((int32_t) (PWM_timer - HAL_GetTick()) > 0) break;

             // temporarily disable outlier test                        // delete
             if (calibration_outlier_fault)                             // delete
               __NOP();                                                 // delete
             calibration_outlier_fault = 0ul;                           // delete

             // if each emitter pair is both bad, assume it's the detector, not the emitters
             mask               = 3ul << 30;
             Emitter_LEDs_red   = ~0ul;
                     LEDs_red   = ~0ul;
             Emitter_LEDs_green = cal_check_result                      // 0-bits are "out of calibration errors" during calibration
                                & (~calibration_outlier_fault);         // 1-bits are "outlier errors" during calibration
                     LEDs_green = ~0ul;
             Emitter_LEDs_blue  = 0;
                     LEDs_blue  = 0;

             // as far as we know, outlier errors indicate a problem with the emitter side,
             // so do not transfer faults to the detector side if outlier errors occurred
             // because it would be misleading
             if (calibration_outlier_fault == 0)
             for (i = 0; i < 16; i++) {
               if ((Emitter_LEDs_green & mask) == 0) {
                 // both emitters in the pair show NG, transfer the fault to the detector side
                 Emitter_LEDs_green |=  mask;
                         LEDs_green &= ~mask;
                 }
               mask >>= 2;
               }

             packet_send_count++;
             LightShow_POST_update_request++;

             // show cal LEDs for 1 second
             PWM_timer = HAL_GetTick() + 1000;

             // stop if cal error
             if ((Emitter_LEDs_green != ~0ul) || (LEDs_green != ~0ul)) state = -2;
             else                                                      state++;
             break;
    case  2: // show cal LEDs for 1000 ms, then start next test
             if ((int32_t) (PWM_timer - HAL_GetTick()) > 0) break;

             // set LED mask for top-most LED
             mask = 1ul << 31;

             // set emitter index for top-most emitter
             emitter = 0;
             Emitter_power_off(emitter);
             packet_send_count++;

             // emitter 0 off for 50 ms
             PWM_timer = HAL_GetTick() + 50;
             state++;
    case  3: // turn emitter off for 50 ms
             if ((int32_t) (PWM_timer - HAL_GetTick()) > 0) break;
             PWM_timer = HAL_GetTick() + 50;

             // emitter has been off for 50 ms
             // init LEDs if first emitter
             if (emitter == 0) {
               // turn off emitter LEDs
               Emitter_LEDs_red   = 0;
               Emitter_LEDs_green = 0;
               Emitter_LEDs_blue  = 0;

               // detectors are proven to work, so no failures are possible here
               // set the detector LEDs to all green
               LEDs_red   = 0;
               LEDs_green = 0;
               LEDs_blue  = 0;
               }

             // set emitter LED to green for emitter power off     detected
             // set emitter LED to red   for emitter power off not detected
             if (Emitter_off_check(emitter)) Emitter_LEDs_green |= mask;
             else                            Emitter_LEDs_red   |= mask;

             // for now, detectors are assumed to work
             // set the detector LEDs to all green
             LEDs_green |= mask;
             LightShow_POST_update_request++;

             mask >>= 1;

             if (++emitter >= EMITTER_SIZE) {
               // turn on all emitters (restore emitter PWM settings)
               Emitter_all_on();

               // continue to show emitter power off detected results for 1 sec
               PWM_timer = HAL_GetTick() + 1000;

               // re-evaluate power-off PWM results in next state
               state++;
               }
             else Emitter_power_off(emitter);
             packet_send_count++;
             break;

    case  4: // re-evaluate power-off PWM results
             // if each emitter pair is both bad, assume it's the detector, not the emitters
             mask = 3ul << 30;
             for (i = 0; i < 16; i++) {
               if ((Emitter_LEDs_green & mask) == 0) {
                 // both emitters in the pair are NG, transfer the fault to the detector side
                 Emitter_LEDs_red   &= ~mask;
                 Emitter_LEDs_green |=  mask;
                         LEDs_green &= ~mask;
                         LEDs_red   |=  mask;
                 }
               mask >>= 2;
               }
             packet_send_count++;
             LightShow_POST_update_request++;

             // stop if power-off PWM error
             if ((Emitter_LEDs_green != ~0ul) || (LEDs_green != ~0ul)) state = -2;
             else                                                      state++;
             break;
    case  5: // continue to show emitter power off detected results for 1 sec
             if ((int32_t) (PWM_timer - HAL_GetTick()) > 0) break;
             PWM_timer += EmitterSetPWMs_interval_ms;

             // turn off emitter LEDs
             Emitter_LEDs_red   = 0;
             Emitter_LEDs_green = 0;
             Emitter_LEDs_blue  = 0;
             packet_send_count++;

             // set detector LEDs to match
             // signal end of POST
             LEDs_red   = 0;
             LEDs_green = 0;
             LEDs_blue  = 0;
             POST_state = 2;                            // POST is complete
             LightShow_POST_update_request++;

             // resume runtime continuous calibration
             state++;
    case  6: // POST is complete
             // perform regular runtime calibration

             // wait 500 ms, then make adjustment
             if ((int32_t) (PWM_timer - HAL_GetTick()) > 0) break;
             PWM_timer += EmitterSetPWMs_interval_ms;

             // make adjustment
             if (normal_adc_sequence) {
               if (Emitter_power_adjust()) packet_send_count++;
               }
             else state++;
             break;
    case  7: // wait here until normal ADC is selected
             if (normal_adc_sequence) {
               // force sending a packet
               packet_send_count++;
               PWM_timer = HAL_GetTick() + EmitterSetPWMs_interval_ms;
               state--;
               }
             break;
    }

  if ((int16_t) (packet_send_count - packet_sent_count) > 0) {
    packet_sent_count += Send_EmitterSetPWMsMsg(POST_state,
                                                Emitter_LEDs_red,
                                                Emitter_LEDs_green,
                                                Emitter_LEDs_blue);
    }
}

//------------------------------------------------------------------------------
// the emitters are sequenced in a scrambled order to prevent unwanted
// contribution from neighboring emitters to the intended detector
// so, we get to unscramble everything here
// then we use the midpoint of filtered light as the threshold
int16_t GetLightCurtainData(void) {
  int16_t  i;
  uint32_t mask;

  if ((int16_t) (Detector_ADC1_Results_HalfCplt_Cnt_ISR - Detector_ADC1_Results_HalfCplt_Cnt_Task) > 0) {
    // Detector_ADC1_Results_HalfCplt data is ready (light readings, part 1)
    Emitter_Light_ADC[ 0] = Detector_ADC1_Results[ 0];
    Emitter_Light_ADC[17] = Detector_ADC1_Results[ 1];
    Emitter_Light_ADC[ 2] = Detector_ADC1_Results[ 2];
    Emitter_Light_ADC[19] = Detector_ADC1_Results[ 3];
    Emitter_Light_ADC[ 4] = Detector_ADC1_Results[ 4];
    Emitter_Light_ADC[21] = Detector_ADC1_Results[ 5];
    Emitter_Light_ADC[ 6] = Detector_ADC1_Results[ 6];
    Emitter_Light_ADC[23] = Detector_ADC1_Results[ 7];
    Emitter_Light_ADC[ 8] = Detector_ADC1_Results[ 8];
    Emitter_Light_ADC[25] = Detector_ADC1_Results[ 9];
    Emitter_Light_ADC[10] = Detector_ADC1_Results[10];
    Emitter_Light_ADC[27] = Detector_ADC1_Results[11];
    Emitter_Light_ADC[12] = Detector_ADC1_Results[12];
    Emitter_Light_ADC[29] = Detector_ADC1_Results[13];
    Emitter_Light_ADC[14] = Detector_ADC1_Results[14];
    Emitter_Light_ADC[31] = Detector_ADC1_Results[15];
    Detector_ADC1_Results_HalfCplt_Cnt_Task++;
    }

  if ((int16_t) (Detector_ADC2_Results_Cplt_Cnt_ISR - Detector_ADC2_Results_Cplt_Cnt_Task) > 0) {
    // Detector_ADC2_Results_Cplt data is ready (light readings, part 2)
    Emitter_Light_ADC[16] = Detector_ADC2_Results[16];    
    Emitter_Light_ADC[ 1] = Detector_ADC2_Results[17];
    Emitter_Light_ADC[18] = Detector_ADC2_Results[18];
    Emitter_Light_ADC[ 3] = Detector_ADC2_Results[19];
    Emitter_Light_ADC[20] = Detector_ADC2_Results[20];
    Emitter_Light_ADC[ 5] = Detector_ADC2_Results[21];
    Emitter_Light_ADC[22] = Detector_ADC2_Results[22];
    Emitter_Light_ADC[ 7] = Detector_ADC2_Results[23];
    Emitter_Light_ADC[24] = Detector_ADC2_Results[24];
    Emitter_Light_ADC[ 9] = Detector_ADC2_Results[25];
    Emitter_Light_ADC[26] = Detector_ADC2_Results[26];
    Emitter_Light_ADC[11] = Detector_ADC2_Results[27];
    Emitter_Light_ADC[28] = Detector_ADC2_Results[28];
    Emitter_Light_ADC[13] = Detector_ADC2_Results[29];
    Emitter_Light_ADC[30] = Detector_ADC2_Results[30];
    Emitter_Light_ADC[15] = Detector_ADC2_Results[31];

    if (Normal_ADC_Sequence) {
      // normal operation
      // in CubeMX, 
      // change ADC 1 to convert 0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
      // change ADC 2 to convert 8, 0, 9, 1, 10, 2, 11, 3, 12, 4, 13, 5, 14, 6, 15, 7
      // we have the complete data set, we can check threshold and construct the bit vector
      // use filtered Light/2
      // the output is a bit vector, one bit per emitter
      // for all detectors, determine if emitter / detector is blocked
      // filter the Light data only when no ball is detected
      // (suspend filtering if Emitter_Vector bit[i] == 0)
      mask = 1;
      for (i = EMITTER_SIZE-1; i >= 0; i--) {
        uint16_t unfiltered_light_ADC = Emitter_Light_ADC[i];
        uint16_t filtered_light_ADC   = (uint32_t) (Emitter_Light_ADC_Q10[i] >> 10);    // filtered ADC

        // look for outliers during POST emitter calibration
        // 1-bits are outlier errors
        if ((filtered_light_ADC   > minimum_threshold) &&
            (unfiltered_light_ADC < blocked_threshold)) {
          calibration_outlier_fault |= mask;
          }

        if ((filtered_light_ADC   <  minimum_threshold) ||                              // if (light reading is less than minimum functional threshold)
            (unfiltered_light_ADC >= recovery_threshold)) {                             // or (no longer blocked)
          // filtered light reading is less than minimum usable light reading
          // or emitter is no longer blocked
          // then indicate not blocked in the bitvector
          // and continue filtering
          Emitter_Vector |= mask;
          Emitter_Light_ADC_Q10[i] = (Emitter_Light_ADC_Q10[i]*1023 + ((uint32_t) unfiltered_light_ADC << 10)) >> 10;
          }
        else if (unfiltered_light_ADC < blocked_threshold) {
          // emitter is blocked
          // indicate blocked in the bitvector
          // and do not filter
          Emitter_Vector &= ~mask;
          }
        mask <<= 1;
        }
      }
    else {
      // bell curve around detector n (0..15, 0 is topmost)
      // in CubeMX, change ADC 1 and ADC 2 to convert IN8 for all conversions
      // filter everything
      // force "no blocked beams" in the light curtain
      Emitter_Vector = 0xfffffffful;
      for (i = 0; i < EMITTER_SIZE; i++) {
        Emitter_Light_ADC_Q10[i] = (Emitter_Light_ADC_Q10[i]*1023 + ((uint32_t) Emitter_Light_ADC[i] << 10)) >> 10;
        }
      }

    // adjust emitter power supply
    // adjust only if (normal ADC sequence) and (no emitters are blocked)
    Emitter_Power_Task(Normal_ADC_Sequence && (Emitter_Vector == 0xfffffffful));

#if 1
    // refill the emitter light msg if it is empty
    if ((int16_t) (Send_EmitterLightDark_ADC_Msg_fill_count - Send_EmitterLightDark_ADC_Msg_empty_count) == 0) {
      mask = 1;
      EmitterLightDark_ADC_Msg.Emitter_Vector = Emitter_Vector;
      for (i = 0; i < EMITTER_SIZE; i++) {
        EmitterLightDark_ADC_Msg.Emitter_Light_ADC_Q0[i] = Emitter_Light_ADC_Q10[31-i] >> 10;
        if (Emitter_Vector & mask) EmitterLightDark_ADC_Msg.Emitter_Dark_ADC_Q0 [i] = 0;
        else                       EmitterLightDark_ADC_Msg.Emitter_Dark_ADC_Q0 [i] = Emitter_Light_ADC[31-i];
        mask <<= 1;
        }
      Send_EmitterLightDark_ADC_Msg_fill_count++;
      }

#else
    // refill the emitter light msg if it is empty (test pattern)
    if ((int16_t) (Send_EmitterLightDark_ADC_Msg_fill_count - Send_EmitterLightDark_ADC_Msg_empty_count) == 0) {
      mask = 1;
      EmitterLightDark_ADC_Msg.Emitter_Vector = 0x7ffffffful;
      for (i = 0; i < EMITTER_SIZE; i++) {
        EmitterLightDark_ADC_Msg.Emitter_Light_ADC_Q0[i] = i << 6;
        EmitterLightDark_ADC_Msg.Emitter_Dark_ADC_Q0 [i] = i << 5;
        mask <<= 1;
        }
      Send_EmitterLightDark_ADC_Msg_fill_count++;
      }
#endif

    Detector_ADC2_Results_Cplt_Cnt_Task++;
    return 1;
    }

  return 0;
}

//------------------------------------------------------------------------------
uint8_t LightCurtainInitialized = 0;

//------------------------------------------------------------------------------
void GetLightCurtainData_Init(void) {
  // debug code to allow us to see the start of initialization on the logic analyzer
  // this makes the first burst envelope start early
  HAL_GPIO_WritePin(ADC_DMA_complete_GPIO_Port, ADC_DMA_complete_Pin, GPIO_PIN_SET);

  // enable detector +5V power supply
  HAL_GPIO_WritePin(EN_5V_PWR_GPIO_Port, EN_5V_PWR_Pin, GPIO_PIN_SET);

  // enable detector -Bias power supply
  HAL_GPIO_WritePin(EN_nBIAS_GPIO_Port, EN_nBIAS_Pin, GPIO_PIN_SET);

  // start timer 8 then immediately disable it
  // CCR1 rising edge triggers ADC SOC
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  TIM8->CR1 &= ~TIM_CR1_CEN;
  TIM8->CNT  = 0;
  TIM8->SR   = 0;

  // start the ADC with DMA
  HAL_ADC_Start_DMA(&hadc1, (uint32_t *) Detector_ADC1_Results, EMITTER_SIZE);
  HAL_ADC_Start_DMA(&hadc2, (uint32_t *) Detector_ADC2_Results, EMITTER_SIZE);

  LightCurtainInitialized = 1;
}

//------------------------------------------------------------------------------
void LightCurtainSync(void) {
  if (LightCurtainInitialized) {
    // ADC and DMA are already configured
    // start the timer
    // CCR1 rising edge triggers ADC SOC
    TIM8->CR1 |= TIM_CR1_CEN;

    HAL_GPIO_WritePin(ADC_DMA_complete_GPIO_Port, ADC_DMA_complete_Pin, GPIO_PIN_SET);
    }
}

//------------------------------------------------------------------------------
void HAL_ADC_Restart_DMA(ADC_HandleTypeDef *hadc, uint32_t Length) {
  DMA_HandleTypeDef *hdma = hadc->DMA_Handle;
  DMA_TypeDef       *regs = (DMA_TypeDef *) hdma->StreamBaseAddress;

  // stop the timer
  TIM8->CR1 &= ~TIM_CR1_CEN;
  TIM8->CNT  = 0;
  TIM8->SR   = 0;

  // stop the ADC and disable DMA mode
  hadc->Instance->CR2 &= ~(ADC_CR2_DMA | ADC_CR2_ADON);

  // clear DMA events
  regs->LIFCR = 0x3FU << hdma->StreamIndex;

  // configure DMA stream data length
  hdma->Instance->NDTR = Length;

  // enable DMA interrupts
  hdma->Instance->CR  |= DMA_SxCR_TCIE | DMA_SxCR_HTIE | DMA_SxCR_EN;

  // enable ADC with DMA
  hadc->Instance->CR2 |= (ADC_CR2_DMA | ADC_CR2_ADON);
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// callbacks

//------------------------------------------------------------------------------
// this overrides a weak callback in stm32f4xx_hal_adc.c
void HAL_ADC_ConvHalfCpltCallback(ADC_HandleTypeDef *hadc) {
  HAL_GPIO_WritePin(ADC_DMA_half_complete_GPIO_Port, ADC_DMA_half_complete_Pin, GPIO_PIN_SET);
  if (hadc == &hadc1) Detector_ADC1_Results_HalfCplt_Cnt_ISR++;
}

//------------------------------------------------------------------------------
// this overrides a weak callback in stm32f4xx_hal_adc.c
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc) {
  if (hadc == &hadc1) {
    HAL_ADC_Restart_DMA(hadc, EMITTER_SIZE);
    HAL_GPIO_WritePin(ADC_DMA_half_complete_GPIO_Port, ADC_DMA_half_complete_Pin, GPIO_PIN_RESET);
    __NOP();
    }
  else if (hadc == &hadc2) {
    Detector_ADC2_Results_Cplt_Cnt_ISR++;
    HAL_ADC_Restart_DMA(hadc, EMITTER_SIZE);
    HAL_GPIO_WritePin(ADC_DMA_complete_GPIO_Port, ADC_DMA_complete_Pin, GPIO_PIN_RESET);
    __NOP();
    }
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// replacement HAL functions

//------------------------------------------------------------------------------
/**
  * @details  Toggles the specified GPIO pins.
  *           This function should override HAL_GPIO_TogglePin from HAL library.
  */
void HAL_GPIO_PDC_TogglePin(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
    // Check the parameters
    assert_param(IS_GPIO_PIN(GPIO_Pin));

    // ATOMICALLY toggle port bit, as opposed to HAL which uses ODR!
    GPIOx->BSRR = ((GPIOx->ODR & GPIO_Pin) ? (uint32_t) GPIO_Pin << 16 : GPIO_Pin);
}
