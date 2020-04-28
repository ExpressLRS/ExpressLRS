/**
  ******************************************************************************
  * @file    stm32_eeprom.h
  * @brief   Header for eeprom module
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2016 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __STM32_EEPROM_H
#define __STM32_EEPROM_H

/* Includes ------------------------------------------------------------------*/
#include "internal.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /************************************************************/
    // Copied from stm32f1xx_hal_flash.h

#if defined(FLASH_BANK2_END)
#define FLASH_FLAG_BSY    FLASH_FLAG_BSY_BANK1    /*!< FLASH Bank1 Busy flag                   */
#define FLASH_FLAG_PGERR  FLASH_FLAG_PGERR_BANK1  /*!< FLASH Bank1 Programming error flag      */
#define FLASH_FLAG_WRPERR FLASH_FLAG_WRPERR_BANK1 /*!< FLASH Bank1 Write protected error flag  */
#define FLASH_FLAG_EOP    FLASH_FLAG_EOP_BANK1    /*!< FLASH Bank1 End of Operation flag       */

#define FLASH_FLAG_BSY_BANK1    FLASH_SR_BSY      /*!< FLASH Bank1 Busy flag                   */
#define FLASH_FLAG_PGERR_BANK1  FLASH_SR_PGERR    /*!< FLASH Bank1 Programming error flag      */
#define FLASH_FLAG_WRPERR_BANK1 FLASH_SR_WRPRTERR /*!< FLASH Bank1 Write protected error flag  */
#define FLASH_FLAG_EOP_BANK1    FLASH_SR_EOP      /*!< FLASH Bank1 End of Operation flag       */

#define FLASH_FLAG_BSY_BANK2    (FLASH_SR2_BSY << 16U)      /*!< FLASH Bank2 Busy flag                   */
#define FLASH_FLAG_PGERR_BANK2  (FLASH_SR2_PGERR << 16U)    /*!< FLASH Bank2 Programming error flag      */
#define FLASH_FLAG_WRPERR_BANK2 (FLASH_SR2_WRPRTERR << 16U) /*!< FLASH Bank2 Write protected error flag  */
#define FLASH_FLAG_EOP_BANK2    (FLASH_SR2_EOP << 16U)      /*!< FLASH Bank2 End of Operation flag       */

#else

#define FLASH_FLAG_BSY    FLASH_SR_BSY      /*!< FLASH Busy flag                          */
#define FLASH_FLAG_PGERR  FLASH_SR_PGERR    /*!< FLASH Programming error flag             */
#define FLASH_FLAG_WRPERR FLASH_SR_WRPRTERR /*!< FLASH Write protected error flag         */
#define FLASH_FLAG_EOP    FLASH_SR_EOP      /*!< FLASH End of Operation flag              */

#endif
#define FLASH_FLAG_OPTVERR ((OBR_REG_INDEX << 8U | FLASH_OBR_OPTERR)) /*!< Option Byte Error        */

#if (defined(STM32F101x6) || defined(STM32F102x6) || defined(STM32F103x6) || defined(STM32F100xB) || defined(STM32F101xB) || defined(STM32F102xB) || defined(STM32F103xB))
#define FLASH_PAGE_SIZE 0x400U
#endif /* STM32F101x6 || STM32F102x6 || STM32F103x6 */
       /* STM32F100xB || STM32F101xB || STM32F102xB || STM32F103xB */

#if (defined(STM32F100xE) || defined(STM32F101xE) || defined(STM32F103xE) || defined(STM32F101xG) || defined(STM32F103xG) || defined(STM32F105xC) || defined(STM32F107xC))
#define FLASH_PAGE_SIZE 0x800U
#endif /* STM32F100xB || STM32F101xB || STM32F102xB || STM32F103xB */
       /* STM32F101xG || STM32F103xG */
       /* STM32F105xC || STM32F107xC */

    /************************************************************/

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#ifndef FLASH_PAGE_SIZE
/*
 * FLASH_PAGE_SIZE is not defined for STM32F2xx, STM32F4xx and STM32F7xx
 * Could be redefined in variant.h or using build_opt.h
 * Warning: This is not the sector size, only the size used for EEPROM
 * emulation. Anyway, all the sector size will be erased.
 * So pay attention to not use this sector for other stuff.
 */
#define FLASH_PAGE_SIZE ((uint32_t)(16 * 1024)) /* 16kB page */
#endif
#define E2END (FLASH_PAGE_SIZE - 1)

    /* Exported macro ------------------------------------------------------------*/
    /* Exported functions ------------------------------------------------------- */

    uint8_t eeprom_read_byte(const uint32_t pos);
    void eeprom_write_byte(uint32_t pos, uint8_t value);

    void eeprom_buffer_fill();
    void eeprom_buffer_flush();
    uint8_t eeprom_buffered_read_byte(const uint32_t pos);
    void eeprom_buffered_write_byte(uint32_t pos, uint8_t value);

#ifdef __cplusplus
}
#endif

#endif /* __STM32_EEPROM_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
