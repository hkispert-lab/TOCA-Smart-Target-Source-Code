// UART.c
// emitter board and detector board RX / TX
// uart communication to PC
//
//#include <string.h>
#include "LEDs.h"
#include "CRC16.h"
#include "LightCurtain.h"
#include "Flash.h"
#include "CRC32.h"
#include "UART.h"

//------------------------------------------------------------------------------
extern UART_HandleTypeDef huart1;       // uart 1 handle
extern UART_HandleTypeDef huart2;       // uart 2 handle
extern UART_HandleTypeDef huart5;       // uart 5 handle
extern ADC_HandleTypeDef  hadc1;        // ADC 1 handle
extern ADC_HandleTypeDef  hadc2;        // ADC 2 handle

uart_t Emitter_Uart1;                   // uart 1 is detector <--> emitter
uart_t Radio_Uart2;                     // uart 2 is detector <--> radio
uart_t PC_Uart5;                        // uart 5 is detector <--> PC

//------------------------------------------------------------------------------
// set normal ADC scan sequence to match emitter scan sequence
bool Normal_ADC_Sequence = true;
void Set_Normal_ADC_Sequence(void) {
  hadc1.Instance->SQR3 = ( 0ul <<  0)   // SQ1
                       | ( 8ul <<  5)   // SQ2
                       | ( 1ul << 10)   // SQ3
                       | ( 9ul << 15)   // SQ4
                       | ( 2ul << 20)   // SQ5
                       | (10ul << 25);  // SQ6
  hadc1.Instance->SQR2 = ( 3ul <<  0)   // SQ7
                       | (11ul <<  5)   // SQ8
                       | ( 4ul << 10)   // SQ9
                       | (12ul << 15)   // SQ10
                       | ( 5ul << 20)   // SQ11
                       | (13ul << 25);  // SQ12
  hadc1.Instance->SQR1 = ( 6ul <<  0)   // SQ13
                       | (14ul <<  5)   // SQ14
                       | ( 7ul << 10)   // SQ15
                       | (15ul << 15)   // SQ16
                       | (15ul << 20);  // L

  hadc2.Instance->SQR3 = ( 8ul <<  0)   // SQ1
                       | ( 0ul <<  5)   // SQ2
                       | ( 9ul << 10)   // SQ3
                       | ( 1ul << 15)   // SQ4
                       | (10ul << 20)   // SQ5
                       | ( 2ul << 25);  // SQ6
  hadc2.Instance->SQR2 = (11ul <<  0)   // SQ7
                       | ( 3ul <<  5)   // SQ8
                       | (12ul << 10)   // SQ9
                       | ( 4ul << 15)   // SQ10
                       | (13ul << 20)   // SQ11
                       | ( 5ul << 25);  // SQ12
  hadc2.Instance->SQR1 = (14ul <<  0)   // SQ13
                       | ( 6ul <<  5)   // SQ14
                       | (15ul << 10)   // SQ15
                       | ( 7ul << 15)   // SQ16
                       | (15ul << 20);  // L
  Normal_ADC_Sequence = true;
}

//------------------------------------------------------------------------------
// set ADC scan sequence to repeat the specified channel
// (read same channel 16 times; used to check emitter signal strength,
// interference from neighboring emitters, alignment issues...)
// channel is 0..15
void Set_Repeating_ADC_Sequence(uint32_t channel) {
  hadc1.Instance->SQR3 = ((1ul << 25) | (1ul << 20) |  (1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // SQ6  .. SQ1
  hadc1.Instance->SQR2 = ((1ul << 25) | (1ul << 20) |  (1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // SQ12 .. SQ7
  hadc1.Instance->SQR1 =               (15ul << 20) | ((1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // L, SQ16 .. SQ13

  hadc2.Instance->SQR3 = ((1ul << 25) | (1ul << 20) |  (1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // SQ6  .. SQ1
  hadc2.Instance->SQR2 = ((1ul << 25) | (1ul << 20) |  (1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // SQ12 .. SQ7
  hadc2.Instance->SQR1 =               (15ul << 20) | ((1ul << 15) | (1ul << 10) | (1ul << 5) | (1ul << 0)) * channel;  // L, SQ16 .. SQ13
  Normal_ADC_Sequence = false;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// TX message senders

//------------------------------------------------------------------------------
// this is used by the ball shadow code
// once when light curtain event is detected
// once after 1 second holdoff for detecting next light curtain event
// send the light show request to emitter
// and start the light show on detector
LightShowRequestMsg_t LightShowRequestMsg = {SYNC_FLAG, e_Light_Show_Request};
void Send_Start_Light_Show(uint16_t selector, uint32_t smear) {
  LightShowRequestMsg.LEDs2                 = smear;                                    // layer 2 LEDs that are currently displayed
//LightShowRequestMsg.RGB2                  = 0;                                        // layer 2 color
  LightShowRequestMsg.ball_crossing_time_ms = RGB_indicators.ball_crossing_time_ms;     // ball-crossing time in 1 ms tick (for e_LightShow_Ball_Crossing)

  LightShowRequestMsg.Selector              = selector;

  LightShowRequestMsg.Header.HeaderCRC      = ~ComputeCRC16_LSBit(&LightShowRequestMsg, sizeof(MsgHeader_t)         - 2, CRC16_INIT);   // compute header CRC
  LightShowRequestMsg.MsgCRC                = ~ComputeCRC16_LSBit(&LightShowRequestMsg, sizeof(LightShowRequestMsg) - 2, CRC16_INIT);   // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &LightShowRequestMsg, sizeof(LightShowRequestMsg));

  LightShow_Selector = selector;
  LightShow_Init     = true;
}

//------------------------------------------------------------------------------
EmitterLightDark_ADC_Msg_t EmitterLightDark_ADC_Msg = {SYNC_FLAG, e_Emitter_Light_Dark_ADC};
int16_t Send_EmitterLightDark_ADC_Msg(void) {
  if (Uart_TX_FIFO_avail(&PC_Uart5) < sizeof(EmitterLightDark_ADC_Msg))
    return 0;

  EmitterLightDark_ADC_Msg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterLightDark_ADC_Msg, sizeof(MsgHeader_t)              - 2, CRC16_INIT);  // compute header CRC
  EmitterLightDark_ADC_Msg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterLightDark_ADC_Msg, sizeof(EmitterLightDark_ADC_Msg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&PC_Uart5, (uint8_t *) &EmitterLightDark_ADC_Msg, sizeof(EmitterLightDark_ADC_Msg));
  return 1;
}

//------------------------------------------------------------------------------
#if 0
EmitterVectorMsg_t EmitterVectorMsg = {SYNC_FLAG, e_Emitter_Vector};
int16_t Send_EmitterVectorMsg(void) {
  if (Uart_TX_FIFO_avail(&PC_Uart5) < sizeof(EmitterVectorMsg))
    return 0;

  EmitterVectorMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterVectorMsg, sizeof(MsgHeader_t)      - 2, CRC16_INIT);  // compute header CRC
  EmitterVectorMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterVectorMsg, sizeof(EmitterVectorMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&PC_Uart5, (uint8_t *) &EmitterVectorMsg, sizeof(EmitterVectorMsg));
  return 1;
}
#endif

//------------------------------------------------------------------------------
// send ball shadow message to PC
BallShadowMsg_t BallShadowMsg = {SYNC_FLAG, e_Ball_Shadow_Detected};
int16_t Send_BallShadowMsg(void) {
  if (Uart_TX_FIFO_avail(&PC_Uart5) < sizeof(BallShadowMsg))
    return 0;

  BallShadowMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&BallShadowMsg, sizeof(MsgHeader_t)     - 2, CRC16_INIT);  // compute header CRC
  BallShadowMsg.MsgCRC           = ~ComputeCRC16_LSBit(&BallShadowMsg, sizeof(BallShadowMsg_t) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&PC_Uart5, (uint8_t *) &BallShadowMsg, sizeof(BallShadowMsg_t));
  return 1;
}

//------------------------------------------------------------------------------
uint32_t Emitter_msg_counter       = 0;
uint32_t Emitter_err_counter       = 0;
uint32_t Emitter_FIFO_full_counter = 0;

EmitterDetectorTestMsg_t EmitterDetectorTestMsg = {SYNC_FLAG, e_Emitter_Detector_Test};
int16_t Send_EmitterDetectorTestMsg(void) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EmitterDetectorTestMsg)) {
    Emitter_FIFO_full_counter++;
    return 0;
    }

  EmitterDetectorTestMsg.test_data[0] = Emitter_msg_counter;
  EmitterDetectorTestMsg.test_data[1] = Emitter_msg_counter+1;

  EmitterDetectorTestMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterDetectorTestMsg, sizeof(MsgHeader_t)            - 2, CRC16_INIT);  // compute header CRC
  EmitterDetectorTestMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterDetectorTestMsg, sizeof(EmitterDetectorTestMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterDetectorTestMsg, sizeof(EmitterDetectorTestMsg));

  return 1;
}

//------------------------------------------------------------------------------
EmitterSetPWMsMsg_t EmitterSetPWMsMsg = {
  .Header             = {SYNC_FLAG, e_Emitter_Set_PWMs},
  .Emitter_LEDs_red   = 0,
  .Emitter_LEDs_green = 0,
  .Emitter_LEDs_blue  = 0,
  .POST_state         = 0,
  .Emitter_slot_rec   = {{ 0, 8},              // slot  0, emitter  0, PWM pct 10
                         {17, 8},              // slot  1, emitter 17, PWM pct 10
                         { 2, 8},              // slot  2, emitter  2, PWM pct 10
                         {19, 8},              // slot  3, emitter 19, PWM pct 10
                         { 4, 8},              // slot  4, emitter  4, PWM pct 10
                         {21, 8},              // slot  5, emitter 21, PWM pct 10
                         { 6, 8},              // slot  6, emitter  6, PWM pct 10
                         {23, 8},              // slot  7, emitter 23, PWM pct 10
                         { 8, 8},              // slot  8, emitter  8, PWM pct 10
                         {25, 8},              // slot  9, emitter 25, PWM pct 10
                         {10, 8},              // slot 10, emitter 10, PWM pct 10
                         {27, 8},              // slot 11, emitter 27, PWM pct 10
                         {12, 8},              // slot 12, emitter 12, PWM pct 10
                         {29, 8},              // slot 13, emitter 29, PWM pct 10
                         {14, 8},              // slot 14, emitter 14, PWM pct 10
                         {31, 8},              // slot 15, emitter 31, PWM pct 10
                         {16, 8},              // slot 16, emitter 16, PWM pct 10
                         { 1, 8},              // slot 17, emitter  1, PWM pct 10
                         {18, 8},              // slot 18, emitter 18, PWM pct 10
                         { 3, 8},              // slot 19, emitter  3, PWM pct 10
                         {20, 8},              // slot 20, emitter 20, PWM pct 10
                         { 5, 8},              // slot 21, emitter  5, PWM pct 10
                         {22, 8},              // slot 22, emitter 22, PWM pct 10
                         { 7, 8},              // slot 23, emitter  7, PWM pct 10
                         {24, 8},              // slot 24, emitter 24, PWM pct 10
                         { 9, 8},              // slot 25, emitter  9, PWM pct 10
                         {26, 8},              // slot 26, emitter 26, PWM pct 10
                         {11, 8},              // slot 27, emitter 11, PWM pct 10
                         {28, 8},              // slot 28, emitter 28, PWM pct 10
                         {13, 8},              // slot 29, emitter 13, PWM pct 10
                         {30, 8},              // slot 30, emitter 30, PWM pct 10
                         {15, 8}}              // slot 31, emitter 15, PWM pct 10
};

int16_t Send_EmitterSetPWMsMsg(uint16_t POST_state,
                               uint32_t Emitter_LEDs_red,
                               uint32_t Emitter_LEDs_green,
                               uint32_t Emitter_LEDs_blue) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EmitterSetPWMsMsg))
    return 0;

  EmitterSetPWMsMsg.POST_state         = POST_state;
  EmitterSetPWMsMsg.Emitter_LEDs_red   = Emitter_LEDs_red;
  EmitterSetPWMsMsg.Emitter_LEDs_green = Emitter_LEDs_green;
  EmitterSetPWMsMsg.Emitter_LEDs_blue  = Emitter_LEDs_blue;

  EmitterSetPWMsMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterSetPWMsMsg, sizeof(MsgHeader_t)       - 2, CRC16_INIT);  // compute header CRC
  EmitterSetPWMsMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterSetPWMsMsg, sizeof(EmitterSetPWMsMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterSetPWMsMsg, sizeof(EmitterSetPWMsMsg));
  return 1;
}

//------------------------------------------------------------------------------
EmitterSetLEDsMsg_t EmitterSetLEDsMsg = {
  .Header = {SYNC_FLAG, e_Emitter_Set_LEDs}
};

int16_t Send_EmitterSetLEDsMsg(void) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EmitterSetLEDsMsg))
    return 0;

  EmitterSetLEDsMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterSetLEDsMsg, sizeof(MsgHeader_t)       - 2, CRC16_INIT);  // compute header CRC
  EmitterSetLEDsMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterSetLEDsMsg, sizeof(EmitterSetLEDsMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterSetLEDsMsg, sizeof(EmitterSetLEDsMsg));
  return 1;
}

//------------------------------------------------------------------------------
EmitterCodeImageMsg_t EmitterCodeImageMsg = {
  .Header = {SYNC_FLAG, e_Emitter_Code_Image}
};

int16_t Send_EmitterCodeImageMsg(uint16_t packet_number, uint32_t *data) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EmitterCodeImageMsg))
    return 0;

  EmitterCodeImageMsg.packet_number = packet_number;
  EmitterCodeImageMsg.data[0]       = data[0];
  EmitterCodeImageMsg.data[1]       = data[1];
  EmitterCodeImageMsg.data[2]       = data[2];
  EmitterCodeImageMsg.data[3]       = data[3];

  EmitterCodeImageMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterCodeImageMsg, sizeof(MsgHeader_t)         - 2, CRC16_INIT);  // compute header CRC
  EmitterCodeImageMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterCodeImageMsg, sizeof(EmitterCodeImageMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterCodeImageMsg, sizeof(EmitterCodeImageMsg));
  return 1;
}

//------------------------------------------------------------------------------
// force the emitter to erase its secondary image flash
EraseSecondaryFlashMsg_t EraseSecondaryFlashMsg = {
  .Header = {SYNC_FLAG, e_Erase_Secondary_Flash}
};

int16_t Send_EraseSecondaryFlashMsg(void) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EraseSecondaryFlashMsg))
    return 0;

  EraseSecondaryFlashMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EraseSecondaryFlashMsg, sizeof(MsgHeader_t)            - 2, CRC16_INIT);  // compute header CRC
  EraseSecondaryFlashMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EraseSecondaryFlashMsg, sizeof(EraseSecondaryFlashMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EraseSecondaryFlashMsg, sizeof(EraseSecondaryFlashMsg));
  return 1;
}

//------------------------------------------------------------------------------
// force the emitter to erase its secondary image flash and copy its ram code image into the secondary image flash
EmitterRestartNowMsg_t EmitterRestartNowMsg = {
  .Header = {SYNC_FLAG, e_Emitter_Restart_Now}
};

int16_t Send_EmitterRestartNowMsg(void) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(EmitterRestartNowMsg))
    return 0;

  EmitterRestartNowMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterRestartNowMsg, sizeof(MsgHeader_t)          - 2, CRC16_INIT);  // compute header CRC
  EmitterRestartNowMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterRestartNowMsg, sizeof(EmitterRestartNowMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterRestartNowMsg, sizeof(EmitterRestartNowMsg));
  return 1;
}

//------------------------------------------------------------------------------
SetSerialNumberMsg_t SetSerialNumberMsg = {
  .Header = {SYNC_FLAG, e_Set_Serial_Number}
};

int16_t Send_Serial_Number(void) {
  if (Uart_TX_FIFO_avail(&PC_Uart5) < sizeof(SetSerialNumberMsg))
    return 0;

  // read serial number from FRAM
  Read_SN();

  // copy serial number into message
  SetSerialNumberMsg.serial_number = Serial_Number;

  // send serial number back to PC
  SetSerialNumberMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&SetSerialNumberMsg, sizeof(MsgHeader_t)        - 2, CRC16_INIT);  // compute header CRC
  SetSerialNumberMsg.MsgCRC           = ~ComputeCRC16_LSBit(&SetSerialNumberMsg, sizeof(SetSerialNumberMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&PC_Uart5, (uint8_t *) &SetSerialNumberMsg, sizeof(SetSerialNumberMsg));
  return 1;
}

//------------------------------------------------------------------------------
void code_image_CRC32(uint32_t *crc, uint32_t start_addr, uint32_t size) {
  uint32_t end_addr   = start_addr + size - 5;                  // backup over CRC to last byte of code image
  uint32_t crc32_addr = start_addr + size - 4;                  // backup to first byte of CRC
  *crc                = * (uint32_t *) crc32_addr;              // assume CRC is OK
  if (!CheckCRC32_MSBit(start_addr, end_addr, *crc)) {          // check the CRC on this region
    *crc = ~0ul;                                                // CRC failed, return 0xffffffff
    }
}

//------------------------------------------------------------------------------
// the bootloader       is located in FLASH_SECTOR_0
// the primary   image  is located in FLASH_SECTOR_1
// the secondary image  is located in FLASH_SECTOR_7
EmitterCodeImageCmdMsg_t EmitterCodeImageCmdMsg = {
  .Header = {SYNC_FLAG, e_Emitter_code_image_cmd},
};

void Send_EmitterCodeImageCmdMsg(uint16_t cmd) {
  EmitterCodeImageCmdMsg.cmd = cmd;

  EmitterCodeImageCmdMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterCodeImageCmdMsg, sizeof(MsgHeader_t)            - 2, CRC16_INIT);  // compute header CRC
  EmitterCodeImageCmdMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterCodeImageCmdMsg, sizeof(EmitterCodeImageCmdMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterCodeImageCmdMsg, sizeof(EmitterCodeImageCmdMsg));
}

//------------------------------------------------------------------------------
// this is different from the restart now message
// it does not force the emitter to erase its secondary image flash
// it does not force the emitter to copy its ram code image into the secondary image flash
// it does force the emitter to restart without erasing and flashing
PowerCycleMsg_t PowerCycleMsg = {
  .Header = {SYNC_FLAG, e_Power_Cycle}
};

int16_t Send_PowerCycleMsg(void) {
  if (Uart_TX_FIFO_avail(&Emitter_Uart1) < sizeof(PowerCycleMsg))
    return 0;

  PowerCycleMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&PowerCycleMsg, sizeof(MsgHeader_t)   - 2, CRC16_INIT);  // compute header CRC
  PowerCycleMsg.MsgCRC           = ~ComputeCRC16_LSBit(&PowerCycleMsg, sizeof(PowerCycleMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &PowerCycleMsg, sizeof(PowerCycleMsg));
  return 1;
}

//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// RX message handlers

//------------------------------------------------------------------------------
// forward the light show request to the emitter
// and also play it locally on the detector
void Process_LightShowRequestMsg(void *RX_msg) {
  LightShowRequestMsg_t *LightShowRequestMsg = (LightShowRequestMsg_t *) RX_msg;

  Uart_TX_put_buf(&Emitter_Uart1, RX_msg, sizeof(LightShowRequestMsg_t));

  LightShow_Selector = LightShowRequestMsg->Selector;
  LightShow_Init     = true;
}

//------------------------------------------------------------------------------
// change the selector sequence on the ADC
//
// selector  |  ADC channel      |  Detector (matches PC tool emitter chart)
//    0      |  normal sequence  |
//    1      |       0 (top)     |      15 (top)
//    2      |       1           |      14
//   ...     |      ...          |      ...
//   15      |      14           |       1
//   16      |      15 (bottom)  |       0 (bottom)
void Process_SetADCSequenceMsg(void *RX_msg) {
  SetADCSequenceMsg_t *SetADCSequenceMsg = (SetADCSequenceMsg_t *) RX_msg;
  uint16_t Selector = SetADCSequenceMsg->Selector;

  if      (Selector ==  0) Set_Normal_ADC_Sequence();                   // 0=normal sequence
  else if (Selector <= 16) Set_Repeating_ADC_Sequence(Selector-1);      // 1..16= ADC 0..15
}

//------------------------------------------------------------------------------
// get battery status from detector board
// and forward on to PC
uint16_t battery_current_mAH = 0;                       // accumulated milli amp hours
void Process_BatteryStatusMsg(void *RX_msg) {
  BatteryStatusMsg_t *BatteryStatusMsg = (BatteryStatusMsg_t *) RX_msg;

  // save the battery status
  battery_current_mAH = BatteryStatusMsg->Status_mAH;

  // forward on to the PC
  Uart_TX_put_buf(&PC_Uart5, RX_msg, sizeof(BatteryStatusMsg_t));
}

//------------------------------------------------------------------------------
// loopback test
uint16_t test_msg_count = 0;        // count number of RX messages from emitter
void Process_EmitterDetectorTestMsg(void *RX_msg) {
  EmitterDetectorTestMsg_t *EmitterDetectorTestMsg = (EmitterDetectorTestMsg_t *) RX_msg;

  test_msg_count++;

  // check loopback test message
  if ((EmitterDetectorTestMsg->test_data[0] !=  Emitter_msg_counter) ||
      (EmitterDetectorTestMsg->test_data[1] != (Emitter_msg_counter+1)))
    Emitter_err_counter++;

  // send another test message
//Emitter_msg_counter++;
//Send_EmitterDetectorTestMsg();
}

//------------------------------------------------------------------------------
// push code image to emitter
uint16_t  code_image_packet_send_count = 0;
uint32_t *pCode_image;                          // pointer to code image

//------------------------------------------------------------------------------
// pull code image from emitter to detector
uint16_t emitter_pull_count = 0;                // non-zero when emitter pull request is complete
void Process_EmitterCodeImageMsg(void *RX_msg) {
  EmitterCodeImageMsg_t *pMsg = (EmitterCodeImageMsg_t *) RX_msg;       // pointer to message packet
  uint16_t  index             = pMsg->packet_number;                    // packet number
  uint32_t *data              = (uint32_t *) &pMsg->data;               // pointer to packet data
  uint32_t *code              = &Detector_code_image[index * 4];        // pointer to code image

 // init the code image when first packet of a fresh image
 if (index == 0) {
   uint16_t i;
   for (i = 0; i < EMITTER_IMAGE_SIZE/4; i++) Detector_code_image[i] = ~0ul;
   }

  // index * 16 is byte offset into code array
  // check (index * 16) < sizeof(Detector_code_image)
  if (index < (EMITTER_IMAGE_SIZE/16)) {
    // copy 16 bytes into code image
    code[0] = data[0];
    code[1] = data[1];
    code[2] = data[2];
    code[3] = data[3];
    }

  // the code image is complete when the image CRC has been sent
  // check code image ram buffer after last packet
  if (Detector_code_image[EMITTER_IMAGE_SIZE/4-1] != ~0ul) {
    emitter_pull_count++;
    }
}

//------------------------------------------------------------------------------
// get serial number
void Process_Get_Serial_Number_Msg(void *RX_msg) {
  Send_Serial_Number();
}

//------------------------------------------------------------------------------
// set serial number
void Process_Set_Serial_Number_Msg(void *RX_msg) {
  SetSerialNumberMsg_t *SetSerialNumberMsg = (SetSerialNumberMsg_t *) RX_msg;

  // get new serial number from PC message
  Serial_Number = SetSerialNumberMsg->serial_number;

  // save new serial number to FRAM
  Write_SN();
}

//------------------------------------------------------------------------------
// restart the target
// forward the request to the emitter
// this is not the same as restart now message from the app
// which is used for OTA firmware update
// this message is used by the PC to emulate a power cycle
void Process_Power_Cycle_Msg(void *RX_msg) {
  Uart_TX_put_buf(&Emitter_Uart1, RX_msg, sizeof(PowerCycleMsg_t));
}

//------------------------------------------------------------------------------
// detector sends request, emitter responds after receiving msg from detector
uint32_t emitter_ram_image_crc32       = ~0ul;  // emitter ram array image CRC located in Emitter_code_image
uint32_t emitter_primary_image_crc32   = ~0ul;  // emitter primary   image CRC located in flash sector 1
uint32_t emitter_secondary_image_crc32 = ~0ul;  // emitter secondary image CRC located in flash sector 7

uint16_t code_image_crc_msg_count      = 0;     // non-zero when versions report is received

void Process_CodeImageCRCMsg(void *RX_msg) {
  CodeImageCRCMsg_t *pMsg = (CodeImageCRCMsg_t *) RX_msg;

  emitter_ram_image_crc32       = pMsg->ram_image_crc32;
  emitter_primary_image_crc32   = pMsg->primary_image_crc32;
  emitter_secondary_image_crc32 = pMsg->secondary_image_crc32;

  code_image_crc_msg_count++;
}

//------------------------------------------------------------------------------
// code image download
//------------------------------------------------------------------------------
void Uart_RX_insert_char(uart_t *uart, uint8_t dr) {
  int16_t RX_chars_in_FIFO = uart->RX_fill_count - uart->RX_empty_count;

  // check for free space in the RX FIFO
  if (++RX_chars_in_FIFO <= uart->RX_FIFO_size) {
    // store RX byte into the FIFO (twice)
    // helps with CRC checking and packet processing in the RX parser
    uart->RX_fill_ptr[0]                  = dr;
    uart->RX_fill_ptr[uart->RX_FIFO_size] = dr;

    // advance the fill pointer and counter
    if (++uart->RX_fill_ptr >= uart->RX_end_ptr) uart->RX_fill_ptr -= uart->RX_FIFO_size;
    uart->RX_fill_count++;

    // update max RX count
    if (uart->RX_max_count < RX_chars_in_FIFO) uart->RX_max_count = RX_chars_in_FIFO;
    }
  else uart->RX_overflow = true;
}

//------------------------------------------------------------------------------
// insert buffer of data directly into the RX FIFO
// instead of coming in from the radio via RX interrupt
void Uart_RX_insert_buf(uint8_t *data, int16_t len) {
  uart_t *uart = &Radio_Uart2;

  while (--len >= 0) {
    Uart_RX_insert_char(uart, *data++);
    }
}

//------------------------------------------------------------------------------
typedef __packed struct {
  uint8_t  command;                                                                     // command = 0x85/0x86
  uint16_t packet_number;                                                               // code image address = packet number * 16
  uint32_t data[4];                                                                     // 16 bytes of data
  uint8_t  eof;                                                                         // 0xc5/0xc6=end of packet
} Radio_RX_code_image_Msg_t;

//------------------------------------------------------------------------------
void Process_Emitter_OTA_Code_Image_Msg(void *RX_msg) {
  EmitterOTACodeImageMsg_t *EmitterOTACodeImageMsg = (EmitterOTACodeImageMsg_t *) RX_msg;

  // insert packet received from PC directly into the radio RX FIFO
  Uart_RX_insert_buf((uint8_t *) &EmitterOTACodeImageMsg->command, sizeof(Radio_RX_code_image_Msg_t));
}

//------------------------------------------------------------------------------
void Process_Detector_OTA_Code_Image_Msg(void *RX_msg) {
  DetectorOTACodeImageMsg_t *DetectorOTACodeImageMsg = (DetectorOTACodeImageMsg_t *) RX_msg;

  // insert packet received from PC directly into the radio RX FIFO
  Uart_RX_insert_buf((uint8_t *) &DetectorOTACodeImageMsg->command, sizeof(Radio_RX_code_image_Msg_t));
}

//------------------------------------------------------------------------------
void Compute_code_image_CRCs(void);
void Process_EraseSecondaryFlash_Msg(void *RX_msg) {
  Erase_flash_sector(FLASH_SECTOR_6);           // erase emitter copy 128 kb
  Erase_flash_sector(FLASH_SECTOR_7);           // erase detector secondary 128 kb
  Compute_code_image_CRCs();

  Send_EraseSecondaryFlashMsg();
}

//------------------------------------------------------------------------------
void Process_Restart_Now_Msg(void *RX_msg) {
  RestartNowMsg_t *RestartNowMsg = (RestartNowMsg_t *) RX_msg;

  // insert packet received from PC directly into the radio RX FIFO
  Uart_RX_insert_buf((uint8_t *) &RestartNowMsg->command, sizeof(Radio_RX_code_image_Msg_t));
}

//------------------------------------------------------------------------------
// report code image CRCs to PC
//------------------------------------------------------------------------------
CodeImageCRCsPCMsg_t CodeImageCRCsPCMsg = {
  .Header = {SYNC_FLAG, e_Code_Image_CRCs_PC}
};

//------------------------------------------------------------------------------
// copy null-terminated string from src to dst
// do not copy more than len-1 bytes
// right-fill empty bytes with null
// force last byte to null
void strcpy_s(char *dst, int16_t len, const char *src) {
  // copy null-terminated string and reserve 1 byte at the end for forced null
  while (*src && (len > 1)) {
    *dst++ = *src++;
    len--;
    }

  // right-fill empty bytes with null
  // force last byte to null
  while (len > 0) {
    *dst++ = 0;
    len--;
    }
}

//------------------------------------------------------------------------------
int16_t Send_Code_Image_CRCs_PC(const char *emitter_part_number,
                                const char *detector_part_number,
                                uint16_t    firmware_major_revision,
                                uint16_t    firmware_minor_revision,
                                uint32_t    emitter_primary_image_crc32,        // emitter primary    image CRC located  in emitter  flash sector 1
                                uint32_t    emitter_secondary_image_crc32,      // emitter secondary  image CRC located  in emitter  flash sector 7
                                uint32_t    detector_emitter_image_crc32,       // detector emitter   image CRC located  in detector flash sector 6
                                uint32_t    matching_emitter_version_crc32,     // matching emitter version CRC embedded in detector build
                                uint32_t    detector_primary_image_crc32,       // detector primary   image CRC located  in detector flash sector 1
                                uint32_t    detector_secondary_image_crc32) {   // detector secondary image CRC located  in detector flash sector 7
  if (Uart_TX_FIFO_avail(&PC_Uart5) < sizeof(CodeImageCRCsPCMsg)) return 0;

  // send code image CRCs to PC
  strcpy_s(CodeImageCRCsPCMsg.emitter_part_number,  sizeof(CodeImageCRCsPCMsg.emitter_part_number),  emitter_part_number);
  strcpy_s(CodeImageCRCsPCMsg.detector_part_number, sizeof(CodeImageCRCsPCMsg.detector_part_number), detector_part_number);
  CodeImageCRCsPCMsg.firmware_major_revision        = firmware_major_revision;
  CodeImageCRCsPCMsg.firmware_minor_revision        = firmware_minor_revision;
  CodeImageCRCsPCMsg.emitter_primary_image_crc32    = emitter_primary_image_crc32;
  CodeImageCRCsPCMsg.emitter_secondary_image_crc32  = emitter_secondary_image_crc32;
  CodeImageCRCsPCMsg.detector_emitter_image_crc32   = detector_emitter_image_crc32;
  CodeImageCRCsPCMsg.matching_emitter_version_crc32 = matching_emitter_version_crc32;
  CodeImageCRCsPCMsg.detector_primary_image_crc32   = detector_primary_image_crc32;
  CodeImageCRCsPCMsg.detector_secondary_image_crc32 = detector_secondary_image_crc32;
  CodeImageCRCsPCMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&CodeImageCRCsPCMsg, sizeof(MsgHeader_t)        - 2, CRC16_INIT);  // compute header CRC
  CodeImageCRCsPCMsg.MsgCRC           = ~ComputeCRC16_LSBit(&CodeImageCRCsPCMsg, sizeof(CodeImageCRCsPCMsg) - 2, CRC16_INIT);  // compute packet CRC
  Uart_TX_put_buf(&PC_Uart5, (uint8_t *) &CodeImageCRCsPCMsg, sizeof(CodeImageCRCsPCMsg));

  return 1;
}

//------------------------------------------------------------------------------
void Process_msg(uart_t *uart, void *RX_msg) {
  MsgHeader_t *pMsgHeader = (MsgHeader_t *) RX_msg;

  switch (pMsgHeader->Cmd) {
    case e_Light_Show_Request     : Process_LightShowRequestMsg        (RX_msg); break;
    case e_Set_ADC_Sequence       : Process_SetADCSequenceMsg          (RX_msg); break;
  //case e_Set_Emitter_Voltage    : Process_SetEmitterVoltage          (RX_msg); break;
  //case e_Emitter_Light_Dark_ADC :                                              break;
  //case e_Emitter_Vector         :                                              break;
  //case e_Ball_Shadow_Detected   :                                              break;
    case e_Battery_Status         : Process_BatteryStatusMsg           (RX_msg); break;
    case e_Emitter_Detector_Test  : Process_EmitterDetectorTestMsg     (RX_msg); break;
  //case e_Emitter_Set_PWMs       :                                              break;
  //case e_Emitter_Set_LEDs       :                                              break;
    case e_Emitter_Code_Image     : Process_EmitterCodeImageMsg        (RX_msg); break;
  //case e_Emitter_Restart_Now    :                                              break;
    case e_Get_Serial_Number      : Process_Get_Serial_Number_Msg      (RX_msg); break;
    case e_Set_Serial_Number      : Process_Set_Serial_Number_Msg      (RX_msg); break;
    case e_Power_Cycle            : Process_Power_Cycle_Msg            (RX_msg); break;
  //case e_Emitter_code_image_cmd :                                              break;
    case e_Code_Image_CRCs        : Process_CodeImageCRCMsg            (RX_msg); break;
  //case e_Code_Image_CRCs_PC     :                                              break;
    case e_Emitter_OTA_Code_Image : Process_Emitter_OTA_Code_Image_Msg (RX_msg); break;
    case e_Detector_OTA_Code_Image: Process_Detector_OTA_Code_Image_Msg(RX_msg); break;
    case e_Erase_Secondary_Flash  : Process_EraseSecondaryFlash_Msg    (RX_msg); break;
    case e_Restart_Now            : Process_Restart_Now_Msg            (RX_msg); break;
    }
}

//------------------------------------------------------------------------------
// return total length of packet
// return zero if command is not recognized
uint16_t Uart_Get_RX_Msg_len(void *RX_msg) {
  MsgHeader_t *pMsgHeader = (MsgHeader_t *) RX_msg;

  switch (pMsgHeader->Cmd) {
    case e_Light_Show_Request     : return sizeof(LightShowRequestMsg_t);
    case e_Set_ADC_Sequence       : return sizeof(SetADCSequenceMsg_t);
  //case e_Set_Emitter_Voltage    : return sizeof(SetEmitterVoltageMsg_t);
  //case e_Emitter_Light_Dark_ADC : return sizeof(EmitterLightDark_ADC_Msg_t);
  //case e_Emitter_Vector         : return sizeof(EmitterVectorMsg_t);
  //case e_Ball_Shadow_Detected   : return sizeof(BallShadowMsg_t);
    case e_Battery_Status         : return sizeof(BatteryStatusMsg_t);
    case e_Emitter_Detector_Test  : return sizeof(EmitterDetectorTestMsg_t);
  //case e_Emitter_Set_PWMs       : return sizeof(EmitterSetPWMsMsg_t);
  //case e_Emitter_Set_LEDs       : return sizeof(EmitterSetLEDsMsg_t);
    case e_Emitter_Code_Image     : return sizeof(EmitterCodeImageMsg_t);
  //case e_Emitter_Restart_Now    : return sizeof(EmitterRestartNowMsg_t);
    case e_Get_Serial_Number      : return sizeof(GetSerialNumberMsg_t);
    case e_Set_Serial_Number      : return sizeof(SetSerialNumberMsg_t);
    case e_Power_Cycle            : return sizeof(PowerCycleMsg_t);
  //case e_Emitter_code_image_cmd : return sizeof(EmitterCodeImageCmdMsg_t);
    case e_Code_Image_CRCs        : return sizeof(CodeImageCRCMsg_t);
  //case e_Code_Image_CRCs_PC     : return sizeof(CodeImageCRCsPCMsg_t);
    case e_Emitter_OTA_Code_Image : return sizeof(EmitterOTACodeImageMsg_t);
    case e_Detector_OTA_Code_Image: return sizeof(DetectorOTACodeImageMsg_t);
    case e_Erase_Secondary_Flash  : return sizeof(EraseSecondaryFlashMsg_t);
    case e_Restart_Now            : return sizeof(RestartNowMsg_t);
    }

  return 0;
}

//------------------------------------------------------------------------------
// detector code image CRCs
uint32_t detector_ram_image_crc32       = ~0ul;         // detector ram array image CRC located in Detector_code_image (after pulling an emitter image from the emitter)
uint32_t detector_primary_image_crc32   = ~0ul;         // detector primary   image CRC located in flash sector 1
uint32_t detector_emitter_image_crc32   = ~0ul;         // detector emitter   image CRC located in flash sector 6
uint32_t detector_secondary_image_crc32 = ~0ul;         // detector secondary image CRC located in flash sector 7

// matching emitter version CRC
uint32_t matching_emitter_version_crc32 = ~0ul;         // matching emitter version CRC

void Compute_code_image_CRCs(void) {
  code_image_CRC32((uint32_t *) &detector_ram_image_crc32,       (uint32_t) Detector_code_image, EMITTER_IMAGE_SIZE);
  code_image_CRC32((uint32_t *) &detector_primary_image_crc32,   (uint32_t) ADDR_FLASH_SECTOR_1, DETECTOR_IMAGE_SIZE);
  code_image_CRC32((uint32_t *) &detector_emitter_image_crc32,   (uint32_t) ADDR_FLASH_SECTOR_6, EMITTER_IMAGE_SIZE);
  code_image_CRC32((uint32_t *) &detector_secondary_image_crc32, (uint32_t) ADDR_FLASH_SECTOR_7, DETECTOR_IMAGE_SIZE);

  // the emitter image CRC is embedded in the detector build
  // immediately preceeding the CRC for the detector build
  // [0] is the matching emitter  version CRC32, manually copy / pasted to UART.c
  // [1] is the          detector image   CRC32, automatically computed by linker
  matching_emitter_version_crc32 = Emitter_detector_matching_version_CRCs[0];
}

//------------------------------------------------------------------------------
bool detector_secondary_flash_erased;
void Check_detector_secondary_flash_erased(void) {
  uint32_t *Secondary_flash      = (uint32_t *) ADDR_FLASH_SECTOR_7;
   int32_t  Secondary_flash_size = SIZE_FLASH_SECTOR_7 / sizeof(uint32_t);
   int32_t  i;

  detector_secondary_flash_erased = false;
  for (i = 0; i < Secondary_flash_size; i++) {
    if (Secondary_flash[i] != ~0ul) return;
    }

  detector_secondary_flash_erased = true;
}

//------------------------------------------------------------------------------
// report code image CRCs to PC
void Report_Code_Image_CRCs_PC(void) {
  static uint32_t timeout = 0;
  static  int16_t state   = 0;
  uint32_t sys_tick       = HAL_GetTick();

  // report code image CRCs to PC once every second
  switch(state) {
    case  0: if ((int32_t) (timeout - sys_tick) > 0) break;
             timeout += 1000;
             state++;
    case  1: if (Send_Code_Image_CRCs_PC(emitter_part_number,
                                         detector_part_number,
                                         Get_firmware_major_revision(),
                                         Get_firmware_minor_revision(),
                                         emitter_primary_image_crc32,           // emitter primary    image CRC located  in emitter  flash sector 1
                                         emitter_secondary_image_crc32,         // emitter secondary  image CRC located  in emitter  flash sector 7
                                         detector_emitter_image_crc32,          // detector emitter   image CRC located  in detector flash sector 6
                                         matching_emitter_version_crc32,        // matching emitter version CRC embedded in detector build
                                         detector_primary_image_crc32,          // detector primary   image CRC located  in detector flash sector 1
                                         detector_secondary_image_crc32)) {     // detector secondary image CRC located  in detector flash sector 7
               state = 0;
               }
             break;
    }
}

//------------------------------------------------------------------------------
// OTA emitter / detector firmware image mismatch task
// assure that the detector has a valid secondary image in flash sector 7
// assure that the detector has a copy of the matching emitter image in flash sector 6
// assure that the emitter has a valid secondary code image that matches the emitter image in detector flash sector 6

uint16_t erase_flash_sector_6 = 0;
uint16_t erase_flash_sector_7 = 0;

void Versions_task(void) {
  static int16_t state = 0;

  // report code image CRCs to PC once per second
  Report_Code_Image_CRCs_PC();

  switch (state) {
    case -1: // stay here until next reboot
             break;
    case  0: // we are here because we just rebooted

             // for debug and testing only
             // if (erase_flash_sector_6) Erase_flash_sector(FLASH_SECTOR_6);        // 128 kb
             // if (erase_flash_sector_7) Erase_flash_sector(FLASH_SECTOR_7);        // 128 kb

             // compute all detector image CRCs
             Compute_code_image_CRCs();

             // check for erased secondary flash (newly manufactured detector)
             Check_detector_secondary_flash_erased();

             // send version report request message to emitter
             Send_EmitterCodeImageCmdMsg(e_ECIcmd_request_code_image_CRCs);
             state++;

             // following the request for the version report, send test message to emitter
             // emitter will echo the message back to detector
             // when we see the test message come back,
             // we can determine if the emitter is old firmware
             // old emitter firmware does not recognize the version report request, only test message received by detector
             // new emitter firmware does recognize the version report request, report and test message received by detector
    case  1: if (Send_EmitterDetectorTestMsg() == 0) break;     // waiting for space in the TX FIFO
             state++;
    case  2: // wait for test message to be received
             if (test_msg_count == 0) break;                    // test message not received yet

             // force OTA update if both emitter and detector do not have the matching emitter image
             if ((emitter_primary_image_crc32  != matching_emitter_version_crc32) &&
                 (detector_emitter_image_crc32 != matching_emitter_version_crc32)) {
               // forcing the version number to zero will cause the app to start an OTA update
               Force_OTA_update();
               state = -1;
               break;
               }

             // if the detector has the matching emitter image
             // and the emitter does not
             // then push the matching emitter image to the emitter
             if ((detector_emitter_image_crc32  == matching_emitter_version_crc32) &&
                 (emitter_secondary_image_crc32 != matching_emitter_version_crc32)) {
               // push matching emitter image to the emitter
               pCode_image                       = (uint32_t *) ADDR_FLASH_SECTOR_6;
               EmitterCodeImageMsg.packet_number = 0;
               code_image_packet_send_count      = EMITTER_IMAGE_SIZE/16;
               state = 3;
               break;
               }

             // if the emitter has the matching emitter image
             // and the detector does not
             // then pull the matching emitter image from the emitter
             if ((emitter_primary_image_crc32  == matching_emitter_version_crc32) &&
                 (detector_emitter_image_crc32 != matching_emitter_version_crc32)) {
               // pull the matching emitter image from the emitter
               Send_EmitterCodeImageCmdMsg(e_ECIcmd_request_primary_code_image);
               state = 5;
               break;
               }

             // if the detector primary image is valid
             // and the detector secondary image is erased (newly built detector)
             // then clone the primary image into the secondary slot for redundancy
             if ((detector_primary_image_crc32 != ~0ul) &&      // valid primary image
                  detector_secondary_flash_erased) {            // erased secondary slot
               state = 7;
               break;
               }

             // if the detector secondary image is missing
             // then force an OTA update
             // else there is nothing for us to do
             if (detector_secondary_image_crc32 != detector_primary_image_crc32) {
               Force_OTA_update();
               }

             // default
             state = -1;
             break;
    case  3: // push emitter image to the emitter
             if (Uart_TX_FIFO_avail(&Emitter_Uart1) < (sizeof(EmitterCodeImageMsg) +            // enough room for the code image packet
                                                       sizeof(EmitterSetPWMsMsg_t))) break;     // and enough room for the set PWM adjustment (with POST LED effects)

             // skip this packet if all 16 bytes are 0xff
             if ((pCode_image[0] != ~0ul) ||
                 (pCode_image[1] != ~0ul) ||
                 (pCode_image[2] != ~0ul) ||
                 (pCode_image[3] != ~0ul)) {

               // copy 16 bytes into code image
               EmitterCodeImageMsg.data[0] = pCode_image[0];
               EmitterCodeImageMsg.data[1] = pCode_image[1];
               EmitterCodeImageMsg.data[2] = pCode_image[2];
               EmitterCodeImageMsg.data[3] = pCode_image[3];

               EmitterCodeImageMsg.Header.HeaderCRC = ~ComputeCRC16_LSBit(&EmitterCodeImageMsg, sizeof(MsgHeader_t)         - 2, CRC16_INIT);  // compute header CRC
               EmitterCodeImageMsg.MsgCRC           = ~ComputeCRC16_LSBit(&EmitterCodeImageMsg, sizeof(EmitterCodeImageMsg) - 2, CRC16_INIT);  // compute packet CRC
               Uart_TX_put_buf(&Emitter_Uart1, (uint8_t *) &EmitterCodeImageMsg, sizeof(EmitterCodeImageMsg));
               }

             EmitterCodeImageMsg.packet_number++;
             pCode_image += 4;

             if (--code_image_packet_send_count == 0) state++;
             break;
    case  4: // send restart message to the emitter
             // this forces the emitter to erase his secondary image
             // and flash the contents of his ram code image buffer into his secondary
             if (Send_EmitterRestartNowMsg()) state = -1;
             break;
    case  5: // pull emitter image from the emitter
             if (emitter_pull_count == 0) break;        // wait for pull request complete

             // pull request complete, erase flash sector 6
             Erase_flash_sector(FLASH_SECTOR_6);        // 128 kb

             // copy code image to emitter code image in sector 6
             Program_flash_sector_word(ADDR_FLASH_SECTOR_6, (uint32_t) &Detector_code_image, EMITTER_IMAGE_SIZE/4);
             state++;
    case  6: // we want to restart the emitter (which also forces a restart of the detector)
             // so we can reevaluate the situation and see if a push back to the emitter is required
             // but... we want to use the cycle power message instead of the emitter restart now message
             // because we don't want the emitter to erase or flash anything after a pull is finished
             // (an emitter restart now message erases the emitter secondary image
             //  and copies the emitter ram code image buffer (contains junk!) to the emitter secondary, which WE DO NOT WANT!)
             // so, using the power cycle message does what we want, and avoids doing what we don't want
             if (Send_PowerCycleMsg()) state = -1;
             break;
    case  7: // clone detector primary into detector secondary after newly built detector
             // erase flash sector 7
             Erase_flash_sector(FLASH_SECTOR_7);        // 128 kb

             // copy code image to emitter code image in sector 6
             Program_flash_sector_word(ADDR_FLASH_SECTOR_7, ADDR_FLASH_SECTOR_1, SIZE_FLASH_SECTOR_7 / sizeof(uint32_t));
             state++;
    case  8: // we want to restart the emitter (which also forces a restart of the detector)
             // so we can reevaluate the situation
             // but... we want to use the cycle power message instead of the emitter restart now message
             // because we don't want the emitter to erase or flash anything
             // (an emitter restart now message erases the emitter secondary image
             //  and copies the emitter ram code image buffer (contains junk!) to the emitter secondary, which WE DO NOT WANT!)
             // so, using the power cycle message does what we want, and avoids doing what we don't want
             if (Send_PowerCycleMsg()) state = -1;
             break;
    }
}

//------------------------------------------------------------------------------
#define EMITTER_TX_FIFO_SIZE (110)
#define EMITTER_RX_FIFO_SIZE (100)

uint8_t EMITTER_TX_FIFO[1*EMITTER_TX_FIFO_SIZE];
uint8_t EMITTER_RX_FIFO[2*EMITTER_RX_FIFO_SIZE];        // size must be doubled to accomodate double-store

void Emitter_Uart1_Init(void) {
  Uart_Init(&Emitter_Uart1,                             // struct
            &huart1,                                    // handle
            EMITTER_TX_FIFO, EMITTER_TX_FIFO_SIZE,
            EMITTER_RX_FIFO, EMITTER_RX_FIFO_SIZE);
}

//------------------------------------------------------------------------------
#define RADIO_TX_FIFO_SIZE (80)                         // large enough for 2 messages (a message is 20 bytes)
#define RADIO_RX_FIFO_SIZE (80)                         // large enough for 4 messages (a message is 20 bytes)

uint8_t RADIO_TX_FIFO[1*RADIO_TX_FIFO_SIZE];
uint8_t RADIO_RX_FIFO[2*RADIO_RX_FIFO_SIZE];            // size must be doubled to accomodate double-store

void Radio_Uart2_Init(void) {
  Uart_Init(&Radio_Uart2,                               // struct
            &huart2,                                    // handle
            RADIO_TX_FIFO, RADIO_TX_FIFO_SIZE,
            RADIO_RX_FIFO, RADIO_RX_FIFO_SIZE);
}

//------------------------------------------------------------------------------
#define PC_TX_FIFO_SIZE (sizeof(BallShadowMsg) + 200)
#define PC_RX_FIFO_SIZE (100)

uint8_t PC_TX_FIFO[1*PC_TX_FIFO_SIZE];
uint8_t PC_RX_FIFO[2*PC_RX_FIFO_SIZE];                  // size must be doubled to accomodate double-store

void PC_Uart5_Init(void) {
  Uart_Init(&PC_Uart5,                                  // struct
            &huart5,                                    // handle
            PC_TX_FIFO, PC_TX_FIFO_SIZE,
            PC_RX_FIFO, PC_RX_FIFO_SIZE);
}

//------------------------------------------------------------------------------
// send ball shadow message to PC for light curtain event
void Send_BallShadow_task(int16_t light_curtain_event) {
  static uint16_t packets_requested = 0;
  static uint16_t packets_sent      = 0;

  if (light_curtain_event) packets_requested++;

  if ((int16_t) (packets_requested - packets_sent) > 0) {
    packets_sent += Send_BallShadowMsg();
    }
}

//------------------------------------------------------------------------------
// send emitter light and dark ADC values twice per second
// message is ready to send when ((fill_count - empty_count) > 0) and 500 ms timeout
uint16_t Send_EmitterLightDark_ADC_Msg_fill_count  = 0;
uint16_t Send_EmitterLightDark_ADC_Msg_empty_count = 0;

void Send_EmitterLightDark_ADCs_task(void) {
  static int16_t  state = -1;
  static uint32_t timeout;

  switch (state) {
    case -1: // init
             timeout = HAL_GetTick();
             state++;
    case  0: // output emitter light and dark ADC values on each timeout
             if (((int16_t) (timeout - HAL_GetTick()) <= 0)                                                             &&
                 ((int16_t) (Send_EmitterLightDark_ADC_Msg_fill_count - Send_EmitterLightDark_ADC_Msg_empty_count) > 0) &&
                 Send_EmitterLightDark_ADC_Msg()) {
               Send_EmitterLightDark_ADC_Msg_empty_count++;
               timeout += 500;
               }
             break;
    }
}

//------------------------------------------------------------------------------
// send emitter vector when changed, but no more often than every 100 ms
void Send_EmitterVector_task(uint32_t vector) {
#if 0
  static int16_t  state = -1;
  static uint32_t timeout;

  vector = 0xfffffffful;                                                        // ??? debug force all green horizontal bars on PC tool
  switch (state) {
    case -1: // init
             EmitterVectorMsg.Emitter_Vector = vector;
             timeout                         = HAL_GetTick();
             state++;
    case  0: // wait for holdoff timer
             if ((int16_t) (timeout - HAL_GetTick()) > 0) break;
             state++;
    case  1: // wait for emitter vector changed
             if (EmitterVectorMsg.Emitter_Vector == vector) break;
             EmitterVectorMsg.Emitter_Vector = vector;
             timeout                         = HAL_GetTick() + 100;
             state++;
    case  2: // send emitter vector
             if (Send_EmitterVectorMsg())
               state = 0;
             break;
    }
#endif
}

#if 0
//------------------------------------------------------------------------------
//------------------------------------------------------------------------------
// test code to simulate incoming emitter and detector code image packets
// called from main.c

//------------------------------------------------------------------------------
#define EMITTER_IMAGE_SIZE  (uint32_t) (72 * 1024)               // 72 k bytes  0x12000
#define DETECTOR_IMAGE_SIZE (uint32_t) (96 * 1024)               // 96 k bytes  0x18000
#define EMITTER_INDEX_SIZE  (EMITTER_IMAGE_SIZE  / sizeof(uint32_t))
#define DETECTOR_INDEX_SIZE (DETECTOR_IMAGE_SIZE / sizeof(uint32_t))

Radio_RX_code_image_Msg_t Radio_Emitter_code_image_Msg = {
  .command       = 0x85,
  .packet_number = 0,
  .eof           = 0xc5
};
Radio_RX_code_image_Msg_t Radio_Detector_code_image_Msg = {
  .command       = 0x86,
  .packet_number = 0,
  .eof           = 0xc6
};
Radio_RX_code_image_Msg_t Radio_restart_now_Msg = {
  .command       = 0x87,
  .packet_number = 0,
  .data          = {0, 0, 0, 0},
  .eof           = 0xc7
};

//------------------------------------------------------------------------------
void Simulate_code_image_download(void) {
  static uint32_t *emitter_code;                                // emitter test image located in sector 5 is 72 kb
  static uint32_t *detector_code;                               // detector test image located in sector 6 is 96 kb
  static uint16_t  code_index;
  static int16_t   state = -1;

  if ((Uart_RX_FIFO_count(&Radio_Uart2) > 20) ||                // radio RX FIFO is not almost empty
      (Uart_TX_FIFO_count(&Emitter_Uart1) > 0))                 // emitter TX FIFO is not empty
    return;

  switch (state) {
    case -1: // set state to 0 to start download
             break;
    case  0: // disable radio uart RX interrupt
             // because we would have two writers to the RX FIFO
             // and one writer is connected to an interrupt
             // for testing, just disable the uart to avoid the problem
             Radio_Uart2.Uart_Instance->CR1 &= ~USART_CR1_RXNEIE;

             // init pointers
             emitter_code  = (uint32_t *) ADDR_FLASH_SECTOR_6;
             detector_code = (uint32_t *) ADDR_FLASH_SECTOR_7;
             code_index    = 0;
             state++;
             break;
    case  1: // send emitter code image packet
             if (code_index < EMITTER_INDEX_SIZE) {
               Radio_Emitter_code_image_Msg.data[0] = emitter_code[code_index++];
               Radio_Emitter_code_image_Msg.data[1] = emitter_code[code_index++];
               Radio_Emitter_code_image_Msg.data[2] = emitter_code[code_index++];
               Radio_Emitter_code_image_Msg.data[3] = emitter_code[code_index++];
               if ((Radio_Emitter_code_image_Msg.data[0] != ~0ul) ||
                   (Radio_Emitter_code_image_Msg.data[1] != ~0ul) ||
                   (Radio_Emitter_code_image_Msg.data[2] != ~0ul) ||
                   (Radio_Emitter_code_image_Msg.data[3] != ~0ul)) {
                 Uart_RX_insert_buf((uint8_t *) &Radio_Emitter_code_image_Msg, sizeof(Radio_Emitter_code_image_Msg));
                 }
               Radio_Emitter_code_image_Msg.packet_number++;
               }
             else {
               code_index = 0;
               state++;
               }
             break;
    case  2: // send detector code image packet
             if (code_index < DETECTOR_INDEX_SIZE) {
               Radio_Detector_code_image_Msg.data[0] = detector_code[code_index++];
               Radio_Detector_code_image_Msg.data[1] = detector_code[code_index++];
               Radio_Detector_code_image_Msg.data[2] = detector_code[code_index++];
               Radio_Detector_code_image_Msg.data[3] = detector_code[code_index++];
               if ((Radio_Detector_code_image_Msg.data[0] != ~0ul) ||
                   (Radio_Detector_code_image_Msg.data[1] != ~0ul) ||
                   (Radio_Detector_code_image_Msg.data[2] != ~0ul) ||
                   (Radio_Detector_code_image_Msg.data[3] != ~0ul)) {
                 Uart_RX_insert_buf((uint8_t *) &Radio_Detector_code_image_Msg, sizeof(Radio_Detector_code_image_Msg));
                 }
               Radio_Detector_code_image_Msg.packet_number++;
               }
             else {
               code_index = 0;
               state++;
               }
             break;
    case  3: // send restart-now packet
             Uart_RX_insert_buf((uint8_t *) &Radio_restart_now_Msg, sizeof(Radio_restart_now_Msg));
             state++;
             break;
    }
}
#endif
