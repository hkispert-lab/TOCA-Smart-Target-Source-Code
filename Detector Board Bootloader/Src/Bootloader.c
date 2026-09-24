#include <stdint.h>
#include "stm32f4xx_hal.h"
#include "LEDs.h"
#include "Flash.h"
#include "CRC32.h"
#include "Bootloader.h"

//------------------------------------------------------------------------------
// reset all peripherals to power up state
// to disable interrupt sources before reconfiguring peripherals
void ResetPeripherals(void) {
  __HAL_RCC_AHB1_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB1_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_AHB2_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB2_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_AHB3_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_AHB3_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_APB1_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_APB1_RELEASE_RESET(); __DSB(); __ISB();
  __HAL_RCC_APB2_FORCE_RESET(); __DSB(); __ISB(); __HAL_RCC_APB2_RELEASE_RESET(); __DSB(); __ISB(); 
}

//------------------------------------------------------------------------------
void BootJump(uint32_t Vector_table_addr) {
  uint32_t *Vector_table = (uint32_t *) Vector_table_addr;

  // disable all interrupts
  NVIC->ICER[0] = ~0;
  NVIC->ICER[1] = ~0;
  NVIC->ICER[2] = ~0;
  NVIC->ICER[3] = ~0;
  NVIC->ICER[4] = ~0;
  NVIC->ICER[5] = ~0;
  NVIC->ICER[6] = ~0;
  NVIC->ICER[7] = ~0;

  // clear all pending interrupt requests
  NVIC->ICPR[0] = ~0;
  NVIC->ICPR[1] = ~0;
  NVIC->ICPR[2] = ~0;
  NVIC->ICPR[3] = ~0;
  NVIC->ICPR[4] = ~0;
  NVIC->ICPR[5] = ~0;
  NVIC->ICPR[6] = ~0;
  NVIC->ICPR[7] = ~0;

  // disable interrupt sources before reconfiguring peripherals
  ResetPeripherals();

  // disable SysTick and clear exception pending bit
  SysTick->CTRL = 0;
  SCB->ICSR     = (SCB_ICSR_PENDSTCLR_Msk |
                   SCB_ICSR_PENDSVCLR_Msk);

  // disable fault handlers
  SCB->SHCSR &= ~(SCB_SHCSR_USGFAULTENA_Msk |
                  SCB_SHCSR_BUSFAULTENA_Msk |
                  SCB_SHCSR_MEMFAULTENA_Msk);

  // assure MSP is the active stack
  __set_CONTROL(__get_CONTROL() & ~CONTROL_SPSEL_Msk);

  // load vector table address
  SCB->VTOR = (uint32_t) Vector_table;

  // set MSP from vector table
  __set_MSP(Vector_table[0]);

  // jump to program entry point by using function call indirect through Vector_table[1]
  ((void(*)(void)) Vector_table[1])();
}

//------------------------------------------------------------------------------
// the bootloader       is located in FLASH_SECTOR_0
// the primary   image  is located in FLASH_SECTOR_1
// the secondary image  is located in FLASH_SECTOR_7
//
// at power up:
// check the primary CRC
// check the secondary CRC
// check if primary and secondary images are identical using byte-for-byte comparison
//
#define IMAGE_SIZE (uint32_t) (96 * 1024)                       // 96k bytes    0x18000

//------------------------------------------------------------------------------
// for testing (copy primary image to secondary image)
// bool copy_primary_to_seconary = false;
void Bootloader(void) {
  uint32_t start_addr;
  uint32_t end_addr;
  uint32_t crc32_addr;
  uint32_t crc32;

  bool primary_crc_ok;
  bool secondary_crc_ok;
  bool identical_images;

  int16_t state = 0;

  // for testing (copy primary image to secondary image)
  // if (copy_primary_to_seconary) {
  //   // copy primary code image to the secondary code image
  //   Erase_flash_sector(FLASH_SECTOR_7);
  //   Program_flash_sector_word(ADDR_FLASH_SECTOR_7, ADDR_FLASH_SECTOR_1, IMAGE_SIZE/4);
  //   }

  while (1) {
    Show_Bootloader_LEDs();
  
    switch (state) {
      case  0: // check secondary code image crc
               start_addr       = ADDR_FLASH_SECTOR_7;                          // start of code image
               end_addr         = ADDR_FLASH_SECTOR_7 + IMAGE_SIZE - 5;         // backup over CRC to last byte of code image
               crc32_addr       = ADDR_FLASH_SECTOR_7 + IMAGE_SIZE - 4;         // backup to first byte of CRC
               crc32            = * (uint32_t *) crc32_addr;
               secondary_crc_ok = CheckCRC32_MSBit(start_addr, end_addr, crc32);
               state++;
               break;
      case  1: // check for identical code images
               identical_images = Verify_flash_sector_word(ADDR_FLASH_SECTOR_1, ADDR_FLASH_SECTOR_7, IMAGE_SIZE/4);
               state++;
               break;
      case  2: // if (secondary image crc is OK) and (images are not identical)
               // then erase primary image and copy secondary to primary and jump to primary image
               // else if (primary image crc is OK)
               // then jump to primary image
               // else stop
               if (!secondary_crc_ok || identical_images) {
                 state = 4;
                 break;
                 }
               // (secondary_crc_ok && !identical_images)
               // erase primary image
               // sector 1 through 4 combined is 112 kb
               Erase_flash_sector(FLASH_SECTOR_1);      // 16 kb
               Erase_flash_sector(FLASH_SECTOR_2);      // 16 kb
               Erase_flash_sector(FLASH_SECTOR_3);      // 16 kb
               Erase_flash_sector(FLASH_SECTOR_4);      // 64 kb
               state++;
               break;
      case  3: // copy secondary code image to primary code image
               Program_flash_sector_word(ADDR_FLASH_SECTOR_1, ADDR_FLASH_SECTOR_7, IMAGE_SIZE/4);
               state++;
               break;
      case  4: // check primary code image crc
               start_addr     = ADDR_FLASH_SECTOR_1;                            // start of code image
               end_addr       = ADDR_FLASH_SECTOR_1 + IMAGE_SIZE - 5;           // backup over CRC to last byte of code image
               crc32_addr     = ADDR_FLASH_SECTOR_1 + IMAGE_SIZE - 4;           // backup to first byte of CRC
               crc32          = * (uint32_t *) crc32_addr;
               primary_crc_ok = CheckCRC32_MSBit(start_addr, end_addr, crc32);
               state++;
               break;
      case  5: // if (primary image crc is OK) then jump to primary image
               if (primary_crc_ok) {
                 Hide_Bootloader_LEDs();                // turn off LEDs
                 BootJump(ADDR_FLASH_SECTOR_1);         // and jump to primary image entry point
                 }
               state++;
               break;
      case  6: // stop
               break;
      }
    }
}
