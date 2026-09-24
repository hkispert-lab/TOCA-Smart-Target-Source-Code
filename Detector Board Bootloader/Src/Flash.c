#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "Flash.h"

//------------------------------------------------------------------------------
// FLASH_SECTOR_1 and FLASH_SECTOR_2 contains primary image
// FLASH_SECTOR_7 contains secondary image
// return 0 if OK
// return 1 if error
uint16_t Erase_flash_sector(uint32_t sector) {
  uint32_t SectorError = 0;
  FLASH_EraseInitTypeDef EraseInitStruct;

  // unlock the flash to enable the flash control register access
  HAL_FLASH_Unlock();

  // clear errors
  __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP    |
                         FLASH_FLAG_OPERR  |
                         FLASH_FLAG_WRPERR |
                         FLASH_FLAG_PGAERR |
                         FLASH_FLAG_PGPERR |
                         FLASH_FLAG_PGSERR);

  // erase the flash sector
  EraseInitStruct.TypeErase    = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
  EraseInitStruct.Sector       = sector;
  EraseInitStruct.NbSectors    = 1;
  if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
    // Error occurred while sector erase
    // User can add here some code to deal with this error
    // SectorError will contain the faulty sector and then to know the code error on this sector,
    // user can call function HAL_FLASH_GetError()
    return 1;
    }

  // lock the Flash to disable the flash control register access
  HAL_FLASH_Lock();

  // return OK
  return 0;
}

//------------------------------------------------------------------------------
// return 0 if OK
// return 1 if error
uint16_t Program_flash_sector_word(uint32_t Flash_address, uint32_t Data_address, uint32_t word_count) {
  // unlock the flash to enable the flash control register access
  HAL_FLASH_Unlock();

  while (word_count--) {
    uint32_t data = * (uint32_t *) Data_address;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, Flash_address, data) != HAL_OK) {
      // error occurred while writing data in Flash memory
      return 1;
      }
    Flash_address += 4;
    Data_address  += 4;
    }

  // lock the Flash to disable the flash control register access (recommended
  // to protect the FLASH memory against possible unwanted operation)
  HAL_FLASH_Lock();

  // return OK
  return 0;
}

//------------------------------------------------------------------------------
// return 1 if match
// return 0 if no match
uint16_t Verify_flash_sector_word(uint32_t Flash_address, uint32_t Data_address, uint32_t word_count) {
  uint32_t *addr1 = (uint32_t *) Flash_address;
  uint32_t *addr2 = (uint32_t *) Data_address;

  while (word_count--) {
    if (*addr1++ != *addr2++) return 0;         // return no match
    }

  // return match
  return 1;
}
