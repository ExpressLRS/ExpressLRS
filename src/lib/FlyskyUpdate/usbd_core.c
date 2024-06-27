/**
  ******************************************************************************
  * @file    usbd_core.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   This file provides all the USBD core functions.
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

//#include "Include.h"

#include "usbd_core.h"
#include "usbd_req.h"
#include "usbd_ioreq.h"
#include "wiring_time.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern uint32_t ADDRESS;

/* Private function prototypes -----------------------------------------------*/
static uint8_t USBD_SetupStage(USB_CORE_HANDLE *pdev);
static uint8_t USBD_DataOutStage(USB_CORE_HANDLE *pdev , uint8_t epnum);
static uint8_t USBD_DataInStage(USB_CORE_HANDLE *pdev , uint8_t epnum);
static uint8_t USBD_SOF(USB_CORE_HANDLE  *pdev);
static uint8_t USBD_Reset(USB_CORE_HANDLE  *pdev);
static uint8_t USBD_Suspend(USB_CORE_HANDLE  *pdev);
static uint8_t USBD_Resume(USB_CORE_HANDLE  *pdev);

USBD_DCD_INT_cb_TypeDef USBD_DCD_INT_cb = 
{
  USBD_DataOutStage,
  USBD_DataInStage,
  USBD_SetupStage,
  USBD_SOF,
  USBD_Reset,
  USBD_Suspend,
  USBD_Resume,   
  
};

USBD_DCD_INT_cb_TypeDef  *USBD_DCD_INT_fops = &USBD_DCD_INT_cb;

/* Private functions ---------------------------------------------------------*/
/**
  * @brief  USBD_Init
  *         Initializes the device stack and load the class driver
  * @param  pdev: device instance
  * @param  class_cb: Class callback structure address
  * @param  usr_cb: User callback structure address
  * @retval None
  */
void USBD_Init(USB_CORE_HANDLE *pdev,
               USBD_DEVICE *pDevice,                  
               USBD_Class_cb_TypeDef *class_cb, 
               USBD_Usr_cb_TypeDef *usr_cb)
{
  /* Hardware Init */
  USB_BSP_Init(pdev);  
  
  USBD_DeInit(pdev);
  
  /*Register class and user callbacks */
  pdev->dev.class_cb = class_cb;
  pdev->dev.usr_cb = usr_cb;  
  pdev->dev.usr_device = pDevice;    
  
  /* Update the serial number string descriptor with the data from the unique ID*/
  Get_SerialNum();
  
  /* set USB DEVICE core params */
  DCD_Init(pdev);
  
  /* Upon Init call usr callback */
  pdev->dev.usr_cb->Init();
  
  /* Enable the pull-up */
#ifdef INTERNAL_PULLUP
  DCD_DevConnect(pdev);
#else
 USB_BSP_DevConnect(pdev);
#endif

  delay(1);	
  /* Enable Interrupts */
  USB_BSP_EnableInterrupt(pdev);
}

/**
  * @brief  USBD_DeInit 
  *         Re-Initialize th device library
  * @param  pdev: device instance
  * @retval status: status
  */
USBD_Status USBD_DeInit(USB_CORE_HANDLE *pdev)
{
  /* Software Init */
  
  return USBD_OK;
}

/**
  * @brief  USBD_SetupStage 
  *         Handle the setup stage
  * @param  pdev: device instance
  * @retval status
  */
static uint8_t USBD_SetupStage(USB_CORE_HANDLE *pdev)
{
  USB_SETUP_REQ req;
  
  USBD_ParseSetupRequest(pdev , &req);
  
  switch (req.bmRequest & 0x1F) 
  {
  case USB_REQ_RECIPIENT_DEVICE:   
    USBD_StdDevReq (pdev, &req);
    break;
    
  case USB_REQ_RECIPIENT_INTERFACE:     
    USBD_StdItfReq(pdev, &req);
    break;
    
  case USB_REQ_RECIPIENT_ENDPOINT:        
    USBD_StdEPReq(pdev, &req);   
    break;
    
  default:           
    DCD_EP_Stall(pdev , req.bmRequest & 0x80);
    break;
  }  
  return USBD_OK;
}

/**
  * @brief  USBD_DataOutStage 
  *         Handle data out stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_DataOutStage(USB_CORE_HANDLE *pdev , uint8_t epnum)
{
  USB_EP *ep;
  
  if(epnum == 0) 
  {
    ep = &pdev->dev.out_ep[0];
    if ( pdev->dev.device_state == USB_EP0_DATA_OUT)
    {
      if(ep->rem_data_len > ep->maxpacket)
      {
        ep->rem_data_len -=  ep->maxpacket;
                
        USBD_CtlContinueRx (pdev, 
                            ep->xfer_buff,
                            MIN(ep->rem_data_len ,ep->maxpacket));
      }
      else
      {
        if((pdev->dev.class_cb->EP0_RxReady != NULL)&&
           (pdev->dev.device_status == USB_CONFIGURED))
        {
          pdev->dev.class_cb->EP0_RxReady(pdev); 
        }
        USBD_CtlSendStatus(pdev);
      }
    }
  }
  else if((pdev->dev.class_cb->DataOut != NULL)&&
          (pdev->dev.device_status == USB_CONFIGURED))
  {
    pdev->dev.class_cb->DataOut(pdev, epnum); 
  }  
  return USBD_OK;
}

/**
  * @brief  USBD_DataInStage 
  *         Handle data in stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
static uint8_t USBD_DataInStage(USB_CORE_HANDLE *pdev , uint8_t epnum)
{
  USB_EP *ep;
  
  if(epnum == 0) 
  {
    ep = &pdev->dev.in_ep[0];
    if ( pdev->dev.device_state == USB_EP0_DATA_IN)
    {
      if(ep->rem_data_len > ep->maxpacket)
      {
        ep->rem_data_len -=  ep->maxpacket;
        USBD_CtlContinueSendData (pdev, 
                                  ep->xfer_buff, 
                                  ep->rem_data_len);
      }
      else
      { /* last packet is MPS multiple, so send ZLP packet */
        if((ep->total_data_len % ep->maxpacket == 0) &&
           (ep->total_data_len >= ep->maxpacket) &&
             (ep->total_data_len < ep->ctl_data_len ))
        {
          
          USBD_CtlContinueSendData(pdev , NULL, 0);
          ep->ctl_data_len = 0;
        }
        else
        {
          if((pdev->dev.class_cb->EP0_TxSent != NULL)&&
             (pdev->dev.device_status == USB_CONFIGURED))
          {
            pdev->dev.class_cb->EP0_TxSent(pdev); 
          }          
          USBD_CtlReceiveStatus(pdev);
        }
      }
    }
    else  if ((pdev->dev.device_state == USB_EP0_STATUS_IN)&& (ADDRESS!=0))
    {
      
      DCD_EP_SetAddress(pdev, ADDRESS); 
      ADDRESS = 0;
    }
  }
  else if((pdev->dev.class_cb->DataIn != NULL)&& 
          (pdev->dev.device_status == USB_CONFIGURED))
  {
    pdev->dev.class_cb->DataIn(pdev, epnum); 
  }  
  return USBD_OK;
}

/**
  * @brief  USBD_Reset 
  *         Handle Reset event
  * @param  pdev: device instance
  * @retval status
  */

static uint8_t USBD_Reset(USB_CORE_HANDLE  *pdev)
{

  DCD_PMA_Config(pdev , 0x00 ,USB_SNG_BUF, ENDP0_RX_ADDRESS);
  DCD_PMA_Config(pdev , 0x80 ,USB_SNG_BUF, ENDP0_TX_ADDRESS);

  /* Open EP0 OUT */
  DCD_EP_Open(pdev,
              0x00,
              USB_MAX_EP0_SIZE,
              EP_TYPE_CTRL);
  
  /* Open EP0 IN */
  DCD_EP_Open(pdev,
              0x80,
              USB_MAX_EP0_SIZE,
              EP_TYPE_CTRL);
  
  /* Upon Reset call user call back */
  pdev->dev.device_status = USB_DEFAULT;
  pdev->dev.usr_cb->DeviceReset(pdev->dev.speed);
  
  return USBD_OK;
}

/**
  * @brief  USBD_Resume 
  *         Handle Resume event
  * @param  pdev: device instance
  * @retval status
  */

static uint8_t USBD_Resume(USB_CORE_HANDLE  *pdev)
{
  /* Upon Resume call user call back */
  pdev->dev.usr_cb->DeviceResumed(); 
  pdev->dev.device_status = pdev->dev.device_old_status;   
  return USBD_OK;
}


/**
  * @brief  USBD_Suspend 
  *         Handle Suspend event
  * @param  pdev: device instance
  * @retval status
  */

static uint8_t USBD_Suspend(USB_CORE_HANDLE  *pdev)
{
  pdev->dev.device_old_status = pdev->dev.device_status;
  /*Device is in Suspended State*/
  pdev->dev.device_status  = USB_SUSPENDED;
  /* Upon Resume call user call back */
  pdev->dev.usr_cb->DeviceSuspended(); 
  return USBD_OK;
}


/**
  * @brief  USBD_SOF 
  *         Handle SOF event
  * @param  pdev: device instance
  * @retval status
  */

static uint8_t USBD_SOF(USB_CORE_HANDLE  *pdev)
{
  if(pdev->dev.class_cb->SOF)
  {
    pdev->dev.class_cb->SOF(pdev); 
  }
  return USBD_OK;
}
/**
  * @brief  USBD_SetCfg 
  *        Configure device and start the interface
  * @param  pdev: device instance
  * @param  cfgidx: configuration index
  * @retval status
  */

USBD_Status USBD_SetCfg(USB_CORE_HANDLE  *pdev, uint8_t cfgidx)
{
  pdev->dev.class_cb->Init(pdev, cfgidx); 
  
  /* Upon set config call user call back */
  pdev->dev.usr_cb->DeviceConfigured();
  return USBD_OK; 
}

/**
  * @brief  USBD_ClrCfg 
  *         Clear current configuration
  * @param  pdev: device instance
  * @param  cfgidx: configuration index
  * @retval status: USBD_Status
  */
USBD_Status USBD_ClrCfg(USB_CORE_HANDLE  *pdev, uint8_t cfgidx)
{
  pdev->dev.class_cb->DeInit(pdev, cfgidx);   
  return USBD_OK;
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/

