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

#ifndef FLASH_TYPEPROGRAM_HALFWORD
#define FLASH_TYPEPROGRAM_HALFWORD 0 // should fail
#endif

/* Function pointer for jumping to user application. */
typedef void (*fnc_ptr)(void);

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
  erase_init.PageAddress = address;
#ifdef FLASH_BANK_1
  erase_init.Banks = FLASH_BANK_1;
#endif
  /* Calculate the number of pages from "address" and the end of flash. */
  erase_init.NbPages = (FLASH_BANK1_END - address + 1) / FLASH_PAGE_SIZE;
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
  erase_init.PageAddress = address;
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
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length)
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
          HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, data[i]))
      {
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
  /* Function pointer to the address of the user application. */
  fnc_ptr jump_to_app;
  jump_to_app = (fnc_ptr)(*(volatile uint32_t *)(FLASH_APP_START_ADDRESS + 4u));
  //led_red_state_set(1);
  //HAL_Delay(500);
  HAL_DeInit();
  /* Change the main stack pointer. */
  //__set_MSP(*(volatile uint32_t *)FLASH_APP_START_ADDRESS);

  asm volatile("msr msp, %0" ::"g"(*(volatile uint32_t *)FLASH_APP_START_ADDRESS));
  SCB->VTOR = (__IO uint32_t)(FLASH_APP_START_ADDRESS);
  jump_to_app();
}
