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

extern uart_t Detector_Uart1;                           // uart 1 is emitter <--> detector
extern Emitter_slot_rec_t Emitter_slot_rec_request[];

void Uart_Init          (uart_t             *uart,
                         UART_HandleTypeDef *huart,
                         uint8_t            *tx_fifo,
                         uint16_t            tx_fifo_size,
                         uint8_t            *rx_fifo,
                         uint16_t            rx_fifo_size);
void Uart_ISR           (uart_t             *uart);
int16_t Uart_TX_FIFO_avail(uart_t *uart);
void Uart_TX_put_buf    (uart_t             *uart,
                         uint8_t            *data,
                         int16_t             len);
void RX_ParseMsgTask    (uart_t             *uart,
                         uint32_t            clock_ticks);
void Detector_Uart1_Init(void);
void Battery_monitor_task(void);
void Emitter_code_image_task(void);
void Compute_code_image_CRCs(void);

extern uint16_t EmitterSetLEDs_Msg_fill_count;
extern uint16_t EmitterSetLEDs_Msg_empty_count;

extern LightShowRequestMsg_t LightShowRequestMsg;
extern EmitterSetLEDsMsg_t   EmitterSetLEDs_Msg;

extern uint16_t code_image_crc_msg_count;

#endif

