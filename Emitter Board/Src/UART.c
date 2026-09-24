// UART.c
// emitter board and detector board RX / TX
// uart communication to PC
//

#include <string.h>
#include "CRC16.h"
#include "UART.h"

//------------------------------------------------------------------------------
// UART setup:
//
// Mode: Async
// Hardware Flow Control (RS232) Disable
// Parameter settings:
//      Baud Rate 115200 Bits/s
//      word length 8 bits (including parity)
//      parity none
//      stop bits 1
//      data direction receive and transmit
//      over sampling 16 samples
// User constants none
// NVIC Interrupts USART5 global interrupt enabled
//      do not call HAL handler (call Uart_ISR() instead)
// DMA Settings none
// GPIO Settings:
//      PE7 UART5_RX  Alternate, pullup, very high
//      PE8 UART5_TX  Alternate, pullup, very high

//------------------------------------------------------------------------------
void Uart_Init(uart_t             *uart,
               UART_HandleTypeDef *huart,
               uint8_t            *tx_fifo,
               uint16_t            tx_fifo_size,
               uint8_t            *rx_fifo,
               uint16_t            rx_fifo_size) {
  memset(uart, 0, sizeof(uart_t));
  
  uart->Uart_Instance                       = huart->Instance;  // base address of USART module

  uart->TX_start_ptr                        = tx_fifo;
  uart->TX_end_ptr                          = tx_fifo + tx_fifo_size;
  uart->TX_fill_ptr                         = tx_fifo;
  uart->TX_empty_ptr                        = tx_fifo;
  uart->TX_FIFO_size                        = tx_fifo_size;
//uart->TX_fill_count                       = 0;
//uart->TX_empty_count                      = 0;
//uart->TX_max_count                        = 0;
//uart->TX_overflow                         = false;

  uart->RX_start_ptr                        = rx_fifo;
  uart->RX_end_ptr                          = rx_fifo + rx_fifo_size;
  uart->RX_fill_ptr                         = rx_fifo;
  uart->RX_empty_ptr                        = rx_fifo;
  uart->RX_FIFO_size                        = rx_fifo_size;
//uart->RX_fill_count                       = 0;
//uart->RX_empty_count                      = 0;
//uart->RX_max_count                        = 0;
//uart->RX_overflow                         = false;
//uart->RX_framing_error                    = 0;

//uart->RX_parse_msg_state                  = 0;
//uart->RX_parse_msg_len                    = 0;
//uart->RX_parse_msg_OK_cnt                 = 0;
//uart->RX_parse_msg_timeout                = 0;
//uart->RX_parse_msg_timeout_cnt            = 0;
//uart->RX_parse_msg_invalid_header_err_cnt = 0;
//uart->RX_parse_msg_invalid_msg_err_cnt    = 0;

  uart->Uart_Instance->CR1 |= USART_CR1_RXNEIE;                 // enable RX ints
}

//------------------------------------------------------------------------------
void Uart_ISR(uart_t *uart) {
  uint32_t sr = uart->Uart_Instance->SR;                      // get status register

  // check for RX char ready interrupt
  if (sr & USART_SR_RXNE) {
    uint8_t dr               = uart->Uart_Instance->DR;       // get data register
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

    // check for framing error (from break)
    if (sr & USART_SR_FE) uart->RX_framing_error++;
    }

  // check for TX empty interrupt AND interrupt enabled
  if ((sr & USART_SR_TXE) && (uart->Uart_Instance->CR1 & USART_CR1_TXEIE)) {
    int16_t TX_chars_in_FIFO = uart->TX_fill_count - uart->TX_empty_count;

    // check for chars in the TX FIFO to send
    if (TX_chars_in_FIFO > 0) {
      // send one TX byte from FIFO
      uart->Uart_Instance->DR = *uart->TX_empty_ptr;

      // advance empty counter
      uart->TX_empty_count++;

      // advance the empty pointer
      if (++uart->TX_empty_ptr >= uart->TX_end_ptr) uart->TX_empty_ptr -= uart->TX_FIFO_size;
      }
    else uart->Uart_Instance->CR1 &= ~USART_CR1_TXEIE;        // no more data to send, disable TX ints
    }
}

//------------------------------------------------------------------------------
int16_t Uart_RX_FIFO_count(uart_t *uart) {return                      uart->RX_fill_count - uart->RX_empty_count;}
int16_t Uart_RX_FIFO_avail(uart_t *uart) {return uart->RX_FIFO_size - uart->RX_fill_count + uart->RX_empty_count;}

//------------------------------------------------------------------------------
int16_t Uart_TX_FIFO_count(uart_t *uart) {return                      uart->TX_fill_count - uart->TX_empty_count;}
int16_t Uart_TX_FIFO_avail(uart_t *uart) {return uart->TX_FIFO_size - uart->TX_fill_count + uart->TX_empty_count;}

//------------------------------------------------------------------------------
void Uart_TX_put_char(uart_t *uart, uint8_t data) {
  int16_t TX_chars_in_FIFO = uart->TX_fill_count - uart->TX_empty_count;

  // check for free space in the TX FIFO
  if (++TX_chars_in_FIFO <= uart->TX_FIFO_size) {
    // store TX byte into the FIFO
    uart->TX_fill_ptr[0] = data;

    // advance fill counter
    uart->TX_fill_count++;

    // enable TX interrupts if disabled
    if ((uart->Uart_Instance->CR1 &  USART_CR1_TXEIE) == 0)
         uart->Uart_Instance->CR1 |= USART_CR1_TXEIE;

    // advance the fill pointer
    if (++uart->TX_fill_ptr >= uart->TX_end_ptr) uart->TX_fill_ptr -= uart->TX_FIFO_size;

    // update max TX count
    if (uart->TX_max_count < TX_chars_in_FIFO) uart->TX_max_count = TX_chars_in_FIFO;
    }
  else uart->TX_overflow = true;
}

//------------------------------------------------------------------------------
void Uart_TX_put_buf(uart_t *uart, uint8_t *data, int16_t len) {
  while (--len >= 0) {
    Uart_TX_put_char(uart, *data++);
    }
}

//------------------------------------------------------------------------------
// send break
void Uart_send_break(uart_t *uart) {
  uart->Uart_Instance->CR1 |= USART_CR1_SBK;
}

//------------------------------------------------------------------------------
uint8_t Uart_RX_get_char(uart_t *uart) {
  int16_t RX_chars_in_FIFO = uart->RX_fill_count - uart->RX_empty_count;
  uint8_t data = 0;

  if (RX_chars_in_FIFO) {
    data = uart->RX_empty_ptr[0];

    // advance the empty pointer and counter
    if (++uart->RX_empty_ptr >= uart->RX_end_ptr) uart->RX_empty_ptr -= uart->RX_FIFO_size;
    uart->RX_empty_count++;
    }

  return data;
}

//------------------------------------------------------------------------------
void Uart_RX_discard_chars(uart_t *uart, int16_t char_count) {
  // advance the empty pointer and counter

  uart->RX_empty_ptr += char_count;
  if (uart->RX_empty_ptr >= uart->RX_end_ptr) uart->RX_empty_ptr -= uart->RX_FIFO_size;

  uart->RX_empty_count += char_count;
}

//------------------------------------------------------------------------------
// return the next FIFO byte without advancing the empty pointer
uint8_t Uart_peek_RX_FIFO(uart_t *uart) {
  return uart->RX_empty_ptr[0];
}

//------------------------------------------------------------------------------
void Process_msg            (uart_t *uart, void *RX_msg);
uint16_t Uart_Get_RX_Msg_len(void *RX_msg);

//------------------------------------------------------------------------------
// packet parser
void RX_ParseMsgTask(uart_t *uart, uint32_t clock_ticks) {
  // if we are actively parsing a message (sync byte found, looking for complete message)
  // then check RX timeout
  if (uart->RX_parse_msg_state) {
    if ((int32_t) (uart->RX_parse_msg_timeout - clock_ticks) <= 0) {
      // message parsing timed out
      // reset state machine to restart scanner
      uart->RX_parse_msg_timeout_cnt++;
      uart->RX_parse_msg_state = 0;
      }
    }

  // scan for valid message
  switch (uart->RX_parse_msg_state) {
    default:
    case  0: // scan for sync byte
             while ((Uart_RX_FIFO_count(uart) > 0) && (Uart_peek_RX_FIFO(uart) != SYNC_FLAG)) {
               // not a sync byte, discard the data
               Uart_RX_discard_chars(uart, 1);
               }

             // if there is at least one byte in the RX FIFO
             // then we have found the sync byte
             // else wait for sync byte
             if (Uart_RX_FIFO_count(uart) <= 0) break;

             // we found the sync byte
             // setup timeout so we don't get stuck waiting for bytes that never come
             uart->RX_parse_msg_timeout = clock_ticks + 50;

             // advance to next state
             uart->RX_parse_msg_state++;
    case  1: // collect message header
             if (Uart_RX_FIFO_count(uart) < sizeof(MsgHeader_t)) break;

             // we have enough bytes for the header
             // get message length
             // command byte tells us how many bytes are in the message
             uart->RX_parse_msg_len = Uart_Get_RX_Msg_len(uart->RX_empty_ptr);

             // check header CRC
             // if command is not in range, or CRC error, discard this message
             if (!CheckCRC16_LSBit(uart->RX_empty_ptr, sizeof(MsgHeader_t)) ||
                 (uart->RX_parse_msg_len == 0)) {
               // header is invalid, skip current sync byte and continue scanning for next sync byte
               uart->RX_parse_msg_invalid_header_err_cnt++;
               Uart_RX_discard_chars(uart, 1);
               uart->RX_parse_msg_state = 0;
               break;
               }

             // header CRC and command byte are valid
             // advance to next state
             uart->RX_parse_msg_state++;
    case  2: // collect complete message
             if (Uart_RX_FIFO_count(uart) >= uart->RX_parse_msg_len) {
               // we have a complete message
               // if the CRC is good, we process the packet
               // if the CRC is bad, we discard the packet
               // either way, we still need to restart the scanner to parse the next message
               uart->RX_parse_msg_state = 0;
               
               // check message CRC, including sync byte
               // if CRC error, discard this message
               if (CheckCRC16_LSBit(uart->RX_empty_ptr, uart->RX_parse_msg_len)) {
                 // message CRC is valid, we have a valid header and a valid message
                 // process the message, then discard the message
                 uart->RX_parse_msg_OK_cnt++;
                 Process_msg(uart, uart->RX_empty_ptr);
                 Uart_RX_discard_chars(uart, uart->RX_parse_msg_len);
                 }
               else {
                 // message is invalid, restart the scanner
                 uart->RX_parse_msg_invalid_msg_err_cnt++;
                 Uart_RX_discard_chars(uart, 1);
                 }
               }
             break;
    }
}
