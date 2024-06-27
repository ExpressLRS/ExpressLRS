/**
  ******************************************************************************
  * @file    usb_dcd_int.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   Device Control driver Interrupt management Header file
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
#ifndef USB_DCD_INT_H__
#define USB_DCD_INT_H__

/* Includes ------------------------------------------------------------------*/
#include "usbd_pwr.h"

/* Exported defines ----------------------------------------------------------*/
/* Mask defining which events has to be handled by the device application software */
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM | \
                 CNTR_ESOFM | CNTR_RESETM)
#ifdef LPM_ENABLED
#undef IMR_MSK
#define IMR_MSK (CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM | \
                 CNTR_ESOFM | CNTR_RESETM | CNTR_L1REQM)
#endif

                    

/* Exported types ------------------------------------------------------------*/
typedef struct _USBD_DCD_INT
{
  uint8_t (* DataOutStage) (USB_CORE_HANDLE *pdev , uint8_t epnum);
  uint8_t (* DataInStage)  (USB_CORE_HANDLE *pdev , uint8_t epnum);
  uint8_t (* SetupStage) (USB_CORE_HANDLE *pdev);
  uint8_t (* SOF) (USB_CORE_HANDLE *pdev);
  uint8_t (* Reset) (USB_CORE_HANDLE *pdev);
  uint8_t (* Suspend) (USB_CORE_HANDLE *pdev);
  uint8_t (* Resume) (USB_CORE_HANDLE *pdev);   
  
}USBD_DCD_INT_cb_TypeDef;

extern USBD_DCD_INT_cb_TypeDef *USBD_DCD_INT_fops;

/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */ 
void CTR(void);
void USB_Istr(void);

#endif /* USB_DCD_INT_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
