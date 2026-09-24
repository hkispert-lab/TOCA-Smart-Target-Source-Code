#ifndef __CRC32_H
#define __CRC32_H

#include <stdbool.h>

bool CheckCRC32_MSBit(uint32_t Start_addr, uint32_t End_addr, uint32_t CRC32);

#endif
