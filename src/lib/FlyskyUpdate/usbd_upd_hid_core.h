/**
  ******************************************************************************
  * @file    usbd_hid_core.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   header file for the usbd_hid_core.c file.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */ 
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USB_HID_CORE_H_
#define __USB_HID_CORE_H_

/* Includes ------------------------------------------------------------------*/
#include "usbd_req.h"
#include "usb_update.h"
/* Exported defines ----------------------------------------------------------*/
#define USB_HID_CONFIG_DESC_SIZ       34
#define USB_HID_DESC_SIZ              9

#define HID_DESCRIPTOR_TYPE           0x21
#define HID_REPORT_DESC               0x22


#define HID_REQ_SET_PROTOCOL          0x0B
#define HID_REQ_GET_PROTOCOL          0x03

#define HID_REQ_SET_IDLE              0x0A
#define HID_REQ_GET_IDLE              0x02

#define HID_REQ_SET_REPORT            0x09
#define HID_REQ_GET_REPORT            0x01

#define USB_REPORT_BUFF_SIZE          (USB_HID_UPD_REPORT_SIZE*2)
/* Exported types ------------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
extern USBD_Class_cb_TypeDef  USBD_HID_cb;
extern sUPD_Info UPDInfo;
/* Exported functions ------------------------------------------------------- */ 
 
uint8_t USBD_HID_SendReport (USB_CORE_HANDLE  *pdev, 
                                 uint8_t *report,
                                 uint16_t len);

#ifdef __cplusplus
extern "C"
{
#endif

void USB_IRQHandler(void);
void USB_Init(void);
void USB_UPD_Background(void);

void SYS_EnterUpdate(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif  /* __USB_HID_CORE_H_ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
