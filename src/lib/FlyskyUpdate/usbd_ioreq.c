/**
  ******************************************************************************
  * @file    usbd_ioreq.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   This file provides the IO requests APIs for control endpoints.
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

/* Includes ------------------------------------------------------------------*/
#include "usbd_ioreq.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/
/**
  * @brief  USBD_CtlSendData
  *         send data on the ctl pipe
  * @param  pdev: device instance
  * @param  buff: pointer to data buffer
  * @param  len: length of data to be sent
  * @retval status
  */
USBD_Status  USBD_CtlSendData (USB_CORE_HANDLE  *pdev, 
                               uint8_t *pbuf,
                               uint16_t len)
{
  USBD_Status ret = USBD_OK;
  
  pdev->dev.in_ep[0].total_data_len = len;
  pdev->dev.in_ep[0].rem_data_len   = len;
  pdev->dev.device_state = USB_EP0_DATA_IN;

  DCD_EP_Tx (pdev, 0, pbuf, len);
 
  return ret;
}

/**
  * @brief  USBD_CtlContinueSendData
  *         continue sending data on the ctl pipe
  * @param  pdev: device instance
  * @param  buff: pointer to data buffer
  * @param  len: length of data to be sent
  * @retval status
  */
USBD_Status  USBD_CtlContinueSendData (USB_CORE_HANDLE  *pdev, 
                                       uint8_t *pbuf,
                                       uint16_t len)
{
  USBD_Status ret = USBD_OK;
  
  DCD_EP_Tx (pdev, 0, pbuf, len);
  
  
  return ret;
}

/**
  * @brief  USBD_CtlPrepareRx
  *         receive data on the ctl pipe
  * @param  pdev: USB device instance
  * @param  buff: pointer to data buffer
  * @param  len: length of data to be received
  * @retval status
  */
USBD_Status  USBD_CtlPrepareRx (USB_CORE_HANDLE  *pdev,
                                  uint8_t *pbuf,                                  
                                  uint16_t len)
{
  USBD_Status ret = USBD_OK;
  
  pdev->dev.out_ep[0].total_data_len = len;
  pdev->dev.out_ep[0].rem_data_len   = len;
  pdev->dev.device_state = USB_EP0_DATA_OUT;
  
  DCD_EP_PrepareRx (pdev,
                    0,
                    pbuf,
                    len);
  

  return ret;
}

/**
  * @brief  USBD_CtlContinueRx
  *         continue receive data on the ctl pipe
  * @param  pdev: USB device instance
  * @param  buff: pointer to data buffer
  * @param  len: length of data to be received
  * @retval status
  */
USBD_Status  USBD_CtlContinueRx (USB_CORE_HANDLE  *pdev, 
                                          uint8_t *pbuf,                                          
                                          uint16_t len)
{
  USBD_Status ret = USBD_OK;
  
  DCD_EP_PrepareRx (pdev,
                    0,                     
                    pbuf,                         
                    len);
  return ret;
}
/**
  * @brief  USBD_CtlSendStatus
  *         send zero length packet on the ctl pipe
  * @param  pdev: USB device instance
  * @retval status
  */
USBD_Status  USBD_CtlSendStatus (USB_CORE_HANDLE  *pdev)
{
  USBD_Status ret = USBD_OK;
  pdev->dev.device_state = USB_EP0_STATUS_IN;
  DCD_EP_Tx (pdev,
             0,
             NULL, 
             0); 
  return ret;
}

/**
  * @brief  USBD_CtlReceiveStatus
  *         receive zero length packet on the ctl pipe
  * @param  pdev: USB device instance
  * @retval status
  */
USBD_Status  USBD_CtlReceiveStatus (USB_CORE_HANDLE  *pdev)
{
  USBD_Status ret = USBD_OK;
  pdev->dev.device_state = USB_EP0_STATUS_OUT;  
  DCD_EP_PrepareRx ( pdev,
                    0,
                    NULL,
                    0); 
  
  return ret;
}


/**
  * @brief  USBD_GetRxCount
  *         returns the received data length
  * @param  pdev: USB device instance
  *         epnum: endpoint index
  * @retval Rx Data blength
  */
uint16_t  USBD_GetRxCount (USB_CORE_HANDLE  *pdev , uint8_t epnum)
{
  return pdev->dev.out_ep[epnum].xfer_count;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
