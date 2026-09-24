#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define PACKED __packed
#include "ProtocolMsgs.h"

typedef struct sUart uart_t;
struct sUart {
          USART_TypeDef *Uart_Instance;                 // UART registers base address

          uint8_t       *TX_start_ptr;                  // &TX_FIFO
          uint8_t       *TX_end_ptr;                    // &TX_FIFO[size]
          uint8_t       *TX_fill_ptr;                   // task fills FIFO
          uint8_t       *TX_empty_ptr;                  // ISR drains FIFO
          uint16_t       TX_FIFO_size;                  // size of TX FIFO
          uint16_t       TX_fill_count;
volatile  uint16_t       TX_empty_count;
          uint16_t       TX_max_count;
          bool           TX_overflow;

          uint8_t       *RX_start_ptr;                  // &RX_FIFO
          uint8_t       *RX_end_ptr;                    // &RX_FIFO[size]
          uint8_t       *RX_fill_ptr;                   // ISR fills FIFO
          uint8_t       *RX_empty_ptr;                  // task drains FIFO
          uint16_t       RX_FIFO_size;                  // size of RX FIFO
volatile  uint16_t       RX_fill_count;
          uint16_t       RX_empty_count;
          uint16_t       RX_max_count;
          bool           RX_overflow;
          uint32_t       RX_framing_error;              // break causes framing error

          int16_t        RX_parse_msg_state;
          int16_t        RX_parse_msg_len;
          uint32_t       RX_parse_msg_OK_cnt;
          uint32_t       RX_parse_msg_timeout;
          uint32_t       RX_parse_msg_timeout_cnt;
          uint32_t       RX_parse_msg_invalid_header_err_cnt;
          uint32_t       RX_parse_msg_invalid_msg_err_cnt;
};

extern uart_t Emitter_Uart1;    // uart 1 is detector <--> emitter
extern uart_t Radio_Uart2;      // uart 2 is detector <--> radio
extern uart_t PC_Uart5;         // uart 5 is detector <--> pc

void Uart_Init            (uart_t             *uart,
                           UART_HandleTypeDef *huart,
                           uint8_t            *tx_fifo,
                           uint16_t            tx_fifo_size,
                           uint8_t            *rx_fifo,
                           uint16_t            rx_fifo_size);
void    Uart_ISR          (uart_t             *uart);
int16_t Uart_RX_FIFO_count(uart_t             *uart);
int16_t Uart_TX_FIFO_count(uart_t             *uart);
int16_t Uart_TX_FIFO_avail(uart_t             *uart);
void    Uart_TX_put_buf   (uart_t             *uart,
                           uint8_t            *data,
                           int16_t             len);
void    Emitter_Uart1_Init(void);
void    Emitter_Uart1_Init2(void);
void    Radio_Uart2_Init  (void);
void    PC_Uart5_Init     (void);
void    RX_ParseMsgTask   (uart_t             *uart,
                           uint32_t            clock_ticks);
void    RX_ParseRadioTask (uart_t             *uart,
                           uint32_t            clock_ticks);
void Radio_RX_test        (void);

//------------------------------------------------------------------------------
extern uint16_t Send_EmitterLightDark_ADC_Msg_fill_count;
extern uint16_t Send_EmitterLightDark_ADC_Msg_empty_count;

extern EmitterLightDark_ADC_Msg_t EmitterLightDark_ADC_Msg;
extern BallShadowMsg_t            BallShadowMsg;
extern EmitterSetPWMsMsg_t        EmitterSetPWMsMsg;
extern EmitterSetLEDsMsg_t        EmitterSetLEDsMsg;
extern LightShowRequestMsg_t      LightShowRequestMsg;

extern bool Normal_ADC_Sequence;

extern uint16_t TimeStampCounter_100ms;
extern uint16_t TimeStampCounter_ms;
extern bool     TimeStampCounterPaused;
extern uint16_t Serial_Number;
extern uint16_t code_image_crc_msg_count;
extern uint32_t Detector_code_image[];
extern const uint32_t Emitter_detector_matching_version_CRCs[1];
extern const char *emitter_part_number;
extern const char *detector_part_number;

int16_t Send_EmitterLightDark_ADC_Msg(void);
int16_t Send_BallShadowMsg           (void);
int16_t Send_EmitterSetPWMsMsg       (uint16_t POST_state,
                                      uint32_t Emitter_LEDs_red,
                                      uint32_t Emitter_LEDs_green,
                                      uint32_t Emitter_LEDs_blue);
int16_t Send_EmitterSetLEDsMsg       (void);
int16_t Send_EmitterCodeImageMsg     (uint16_t packet_number, uint32_t *data);
int16_t Send_EmitterRestartNowMsg    (void);

void Send_Start_Light_Show                    (uint16_t selector, uint32_t smear);
void Send_BallShadow_task                     (int16_t light_curtain_event);
void Send_EmitterLightDark_ADCs_task          (void);
void Send_EmitterVector_task                  (uint32_t vector);
void Send_Ball_Crossing_Event                 (uint16_t object_type,
                                               uint16_t elevation_pct,
                                               uint16_t velocity_pct);
void Send_Ball_Crossing_Event_Task            (void);
void Send_Ball_Crossing_Event_Holdoff_complete(void);
void Simulate_code_image_download             (void);
void Versions_task                            (void);

void Read_SN(void);
void Write_SN(void);

void Send_EmitterCodeImageCmdMsg(uint16_t cmd);
void Force_OTA_update(void);
uint16_t Get_firmware_major_revision(void);
uint16_t Get_firmware_minor_revision(void);

#endif
