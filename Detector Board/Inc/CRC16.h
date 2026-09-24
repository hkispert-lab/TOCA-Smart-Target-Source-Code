#ifndef CRC_16H
#define CRC_16H

#include <stdint.h>
#include <stdbool.h>

#define CRC16_INIT         (~0)
uint16_t ComputeCRC16_LSBit(void const *pMsg, int16_t len, uint16_t CRC16);
bool     CheckCRC16_LSBit  (void const *pMsg, int16_t len);
void     CRC_Test          (void);

#endif

