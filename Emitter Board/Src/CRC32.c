#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_crc.h"
#include "CRC32.h"

//------------------------------------------------------------------------------
#define CRC32_REMAINDER_MSBIT  (0xC704DD7BL)                                    ///< CRC32 MSBIT Remainder  (CCITT-32)

//extern uint32_t __checksum_begin; // 0x8004000  address of first byte in code image, inclusive
//extern uint32_t __checksum_end;   // 0x800bffb  address of last byte in the code image, inclusive
//extern uint32_t __checksum;       // 0x800bffc  32-bit CRC computed and appended to the code image

//------------------------------------------------------------------------------
// compute CRC on code image
// Start_addr is address of first byte in code image, inclusive
// End_addr   is address of last byte in the code image, inclusive
// CRC32      is the CRC appended to code image
//
// return true if CRC is OK
// else return false
//
// "1 Emitter linker checksum.png" shows linker settings to compute and append CRC to the code image
// "2 Emitter linker input.png" shows keep symbols options
// "3 Emitter linker icf.png" shows linker command file modifications
bool CheckCRC32_MSBit(uint32_t Start_addr, uint32_t End_addr, uint32_t CRC32) {
  uint32_t *ptr  = (uint32_t *) Start_addr;
  uint32_t *pEnd = (uint32_t *) (End_addr + 1);

  // reset CRC module (init DR to 0xfffffffful)
  CRC->CR = CRC_CR_RESET;             

  // compute the checksum over the region
  while (ptr < pEnd) CRC->DR = *ptr++;

  // add-in the expected CRC result
  CRC->DR = CRC32;

  // return true if CRC is OK, else return false
  return (CRC->DR == CRC32_REMAINDER_MSBIT);
}
