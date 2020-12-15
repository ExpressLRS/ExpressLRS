/**
 * @file    flash.c
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module handles the memory related functions.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#include "flash.h"
#include "main.h"
#include "uart.h"

#ifndef DUMPING
#define DUMPING 0
#endif

#ifndef FLASH_TYPEPROGRAM_HALFWORD
#define FLASH_TYPEPROGRAM_HALFWORD 0 // should fail
#endif

#if /*defined(STM32F3xx) &&*/ defined(FLASH_SIZE_DATA_REGISTER)
uint32_t get_flash_end(void) {
  uint32_t end = FLASH_BASE;
  switch ((*((uint16_t *)FLASH_SIZE_DATA_REGISTER))) {
  case 0x200U:
    end += 0x7FFFFU;
    break;
  case 0x100U:
    end += 0x3FFFFU;
    break;
  case 0x80U:
    end += 0x1FFFFU;
    break;
  case 0x40U:
    end += 0xFFFFU;
    break;
  case 0x20U:
    end += 0x7FFFU;
    break;
  default:
    end += 0x3FFFU;
    break;
  }
  return end;
}
#endif /* FLASH_SIZE_DATA_REGISTER */

/* Function pointer for jumping to user application. */
typedef void (*fnc_ptr)(void);

#if DUMPING
flash_status flash_dump(void)
{
  //__IO uint8_t* address = (uint8_t*)FLASH_BASE;
  uint32_t len = 0x2000;
  uart_transmit_str("Memery dump:\r\n");
  /*while(len--) {

  }*/
  uart_transmit_bytes((uint8_t*)FLASH_BASE, len);
  return FLASH_OK;
}
#else
flash_status flash_dump(void)
{
  return FLASH_OK;
}
#endif


/**
 * @brief   This function erases the memory.
 * @param   address: First address to be erased (the last is the end of the
 * flash).
 * @return  status: Report about the success of the erasing.
 */
flash_status flash_erase(uint32_t address)
{
  flash_status status = FLASH_ERROR;
  FLASH_EraseInitTypeDef erase_init;
  uint32_t error = 0u;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
#if defined(STM32L4xx)
  erase_init.Page = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
#else
  erase_init.PageAddress = address;
#endif
#ifdef FLASH_BANK_1
  erase_init.Banks = FLASH_BANK_1;
#endif
  /* Calculate the number of pages from "address" and the end of flash. */
  erase_init.NbPages = (FLASH_APP_END_ADDRESS - address + 1) / FLASH_PAGE_SIZE;
  /* Do the actual erasing. */
  HAL_FLASH_Unlock();
  if (HAL_OK == HAL_FLASHEx_Erase(&erase_init, &error))
  {
    status = FLASH_OK;
  }
  HAL_FLASH_Lock();

  return status;
}

/**
 * @brief   This function erases the current flash page.
 * @param   address: address to be erased.
 * @return  status: Report about the success of the erasing.
 */
flash_status flash_erase_page(uint32_t address)
{
  flash_status status = FLASH_OK;
  FLASH_EraseInitTypeDef erase_init;
  uint32_t error = 0u;

  erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
#if defined(STM32L4xx)
  erase_init.Page = (address - FLASH_BASE) / FLASH_PAGE_SIZE;
#else
  erase_init.PageAddress = address;
#endif
#ifdef FLASH_BANK_1
  erase_init.Banks = FLASH_BANK_1;
#endif
  erase_init.NbPages = 1;

  /* Do the actual erasing. */
  HAL_FLASH_Unlock();
  if (HAL_OK != HAL_FLASHEx_Erase(&erase_init, &error))
  {
    status = FLASH_ERROR;
  }
  HAL_FLASH_Lock();

  return status;
}

/**
 * @brief   This function flashes the memory.
 * @param   address: First address to be written to.
 * @param   *data:   Array of the data that we want to write.
 * @param   *length: Size of the array.
 * @return  status: Report about the success of the writing.
 */
#if defined(STM32L4xx)
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length) {
  flash_status status = FLASH_OK;

  length = (length + 1) >> 1; // roundup and convert to double words

  HAL_FLASH_Unlock();

  /* Loop through the array. */
  for (uint32_t i = 0u; (i < length) && (FLASH_OK == status); i++) {
    /* If we reached the end of the memory, then report an error and don't
     * do anything else.*/
    if (FLASH_APP_END_ADDRESS <= address) {
      status |= FLASH_ERROR_SIZE;
    } else {
      /* The actual flashing. If there is an error, then report it. */
      if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, address,
                                      *(uint64_t *)data)) {
        status |= FLASH_ERROR_WRITE;
      }
      /* Read back the content of the memory. If it is wrong, then report an
       * error. */
      if (((*data++) != (*(volatile uint32_t *)address)) ||
          ((*data++) != (*(volatile uint32_t *)(address + sizeof(uint32_t))))) {
        status |= FLASH_ERROR_READBACK;
      }

      /* Shift the address by a double word. */
      address += sizeof(uint64_t);
    }
  }

  HAL_FLASH_Lock();

  return status;
}

#else // !STM32L4xx
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length) {
  flash_status status = FLASH_OK;

  HAL_FLASH_Unlock();

  /* Loop through the array. */
  for (uint32_t i = 0u; (i < length) && (FLASH_OK == status); i++)
  {
    /* If we reached the end of the memory, then report an error and don't do
     * anything else.*/
    if (FLASH_APP_END_ADDRESS <= address)
    {
      status |= FLASH_ERROR_SIZE;
    }
    else
    {
      /* The actual flashing. If there is an error, then report it. */
      if (HAL_OK != HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i])) {
        status |= FLASH_ERROR_WRITE;
      }
      /* Read back the content of the memory. If it is wrong, then report an
       * error. */
      if (((data[i])) != (*(volatile uint32_t *)address))
      {
        status |= FLASH_ERROR_READBACK;
      }

      /* Shift the address by a word. */
      address += 4u;
    }
  }

  HAL_FLASH_Lock();

  return status;
}
#endif // STM32L4xx

/**
 * @brief   This function flashes the memory.
 * @param   address: First address to be written to.
 * @param   *data:   Array of the data that we want to write.
 * @param   *length: Size of the array.
 * @return  status: Report about the success of the writing.
 */
flash_status flash_write_halfword(uint32_t address, uint16_t *data,
                                  uint32_t length)
{
  flash_status status = FLASH_OK;

  HAL_FLASH_Unlock();

  /* Loop through the array. */
  for (uint32_t i = 0u; (i < length) && (FLASH_OK == status); i++)
  {
    /* If we reached the end of the memory, then report an error and don't do
     * anything else.*/
    if (FLASH_APP_END_ADDRESS <= address)
    {
      status |= FLASH_ERROR_SIZE;
    }
    else
    {
      /* The actual flashing. If there is an error, then report it. */
      if (HAL_OK !=
          HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, data[i]))
      {
        status |= FLASH_ERROR_WRITE;
      }
      /* Read back the content of the memory. If it is wrong, then report an
       * error. */
      if (((data[i])) != (*(volatile uint16_t *)address))
      {
        status |= FLASH_ERROR_READBACK;
      }

      /* Shift the address by a word. */
      address += 2u;
    }
  }

  HAL_FLASH_Lock();

  return status;
}

/**
 * @brief   Actually jumps to the user application.
 * @param   void
 * @return  void
 */
void flash_jump_to_app(void)
{
  led_state_set(LED_STARTING);

  if (flash_check_app_loaded() < 0) {
    /* Restart if no valid app found */
    NVIC_SystemReset();
  }

  /* Small delay to allow UART TX send out everything */
  HAL_Delay(100);

  /* Function pointer to the address of the user application. */
  fnc_ptr jump_to_app;
  jump_to_app = (fnc_ptr)(*(volatile uint32_t *)(FLASH_APP_START_ADDRESS + 4u));
  /* Remove configs before jump. */
  uart_deinit();
  HAL_DeInit();
  /* Change the main stack pointer. */
  asm volatile("msr msp, %0" ::"g"(*(volatile uint32_t *)FLASH_APP_START_ADDRESS));
  SCB->VTOR = (__IO uint32_t)(FLASH_APP_START_ADDRESS);
  //__set_MSP(*(volatile uint32_t *)FLASH_APP_START_ADDRESS);
  jump_to_app();

  while(1)
    ;
}

int8_t flash_check_app_loaded(void)
{
  /* Check if app is already loaded */
  uintptr_t app_stack = *(volatile uintptr_t *)(FLASH_APP_START_ADDRESS);
  uintptr_t app_reset = *(volatile uintptr_t *)(FLASH_APP_START_ADDRESS + 4u);
  uintptr_t app_nmi = *(volatile uintptr_t *)(FLASH_APP_START_ADDRESS + 8u);
  if (((uint16_t)(app_stack >> 20) == 0x200) &&
      ((uint16_t)(app_reset >> 16) == 0x0800) &&
      ((uint16_t)(app_nmi >> 16) == 0x0800)) {
    return 0;
  }
  return -1;
}
