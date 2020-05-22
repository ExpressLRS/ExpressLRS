/**
 * @file    flash.h
 * @author  Ferenc Nemeth
 * @date    21 Dec 2018
 * @brief   This module handles the memory related functions.
 *
 *          Copyright (c) 2018 Ferenc Nemeth - https://github.com/ferenc-nemeth
 */

#ifndef FLASH_H_
#define FLASH_H_

#ifdef STM32L0xx
#include "stm32l0xx_hal.h"
#elif defined(STM32L1xx)
#include "stm32l1xx_hal.h"
#elif defined(STM32F1)
#include "stm32f1xx_hal.h"
#endif
#include <stdint.h>

/* Start and end addresses of the user application. */
#ifndef FLASH_BASE
#define FLASH_BASE ((uint32_t)0x08000000u)
#endif
#ifndef FLASH_APP_OFFSET
#define FLASH_APP_OFFSET ((uint32_t)0x8000u)
#endif
#define FLASH_APP_START_ADDRESS (FLASH_BASE + FLASH_APP_OFFSET)
#define FLASH_APP_END_ADDRESS ((uint32_t)FLASH_BANK1_END - 0x10u) /**< Leave a little extra space at the end. */

#ifndef FLASH_BANK1_END
#define FLASH_BANK1_END FLASH_END
#endif

/* Status report for the functions. */
#define FLASH_OK 0x00u             /**< The action was successful. */
#ifndef FLASH_ERROR_SIZE
#define FLASH_ERROR_SIZE 0x01u     /**< The binary is too big. */
#endif
#define FLASH_ERROR_WRITE 0x02u    /**< Writing failed. */
#define FLASH_ERROR_READBACK 0x04u /**< Writing was successful, but the content of the memory is wrong. */
#define FLASH_ERROR 0xFFu           /**< Generic error. */

typedef uint8_t flash_status;

flash_status flash_erase(uint32_t address);
flash_status flash_erase_page(uint32_t address);
flash_status flash_write(uint32_t address, uint32_t *data, uint32_t length);
flash_status flash_write_halfword(uint32_t address, uint16_t *data,
                                  uint32_t length);
void flash_jump_to_app(void);
void flash_jump_to_bl(void);

#endif /* FLASH_H_ */
