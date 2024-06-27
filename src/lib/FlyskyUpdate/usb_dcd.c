/**
  ******************************************************************************
  * @file    usb_dcd.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   Device interface layer used by the library to access the core.
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
#include "usb_dcd.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
uint32_t wInterrupt_Mask=0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief Device Initialization
  * @param  pdev: device instance
  * @retval : None
  */
void DCD_Init(USB_CORE_HANDLE *pdev)
{
  /*Device is in Default State*/
  pdev->dev.device_status = USB_DEFAULT;
  pdev->dev.device_address = 0;
  pdev->dev.DevRemoteWakeup = 0;
  
  pdev->dev.speed = USB_SPEED_FULL; /*kept for API compatibility reason*/
  
  /*CNTR_FRES = 1*/
  SetCNTR(CNTR_FRES);
  
  /*CNTR_FRES = 0*/
  SetCNTR(0);
  
  /*Clear pending interrupts*/
  SetISTR(0);
  
  /*Set Btable Address*/
  SetBTABLE(BTABLE_ADDRESS);
  
  /*set wInterrupt_Mask global variable*/
  wInterrupt_Mask = CNTR_CTRM  | CNTR_WKUPM | CNTR_SUSPM | CNTR_ERRM  | CNTR_SOFM \
    | CNTR_ESOFM | CNTR_RESETM;
  
#ifdef LPM_ENABLED
  /* enable L1REQ interrupt */
  wInterrupt_Mask |= CNTR_L1REQM;
  
  /* Enable LPM support and enable ACK answer to LPM request*/
  _SetLPMCSR(LPMCSR_LMPEN | LPMCSR_LPMACK);
#endif
 
  /*Set interrupt mask*/
  SetCNTR(wInterrupt_Mask); 
}


/**
  * @brief Stop device
  * @param  pdev: device instance
  * @retval : None
  */
void DCD_StopDevice(USB_CORE_HANDLE *pdev)
{
    /* disable all interrupts and force USB reset */
  _SetCNTR(CNTR_FRES);
  
  /* clear interrupt status register */
  _SetISTR(0);
  
  /* switch-off device */
  _SetCNTR(CNTR_FRES + CNTR_PDWN);
  
  /*Device is in default state*/
  pdev->dev.device_status  = USB_DEFAULT;
  
}

/**
  * @brief Configure PMA for EP
  * @param  pdev : Device instance
  * @param  ep_addr: endpoint address
  * @param  ep_Kind: endpoint Kind
  *                @arg USB_SNG_BUF: Single Buffer used
  *                @arg USB_DBL_BUF: Double Buffer used
  * @param  pmaadress: EP address in The PMA: In case of single buffer endpoint
  *                   this parameter is 16-bit value providing the address
  *                   in PMA allocated to endpoint.
  *                   In case of double buffer endpoint this parameter
  *                   is a 32-bit value providing the endpoint buffer 0 address
  *                   in the LSB part of 32-bit value and endpoint buffer 1 address
  *                   in the MSB part of 32-bit value.
  * @retval : status
  */

uint32_t DCD_PMA_Config(USB_CORE_HANDLE *pdev , 
                        uint16_t ep_addr,
                        uint16_t ep_kind,
                        uint32_t pmaadress)

{
  USB_EP *ep;
  /* initialize ep structure*/
  if ((ep_addr & 0x80) == 0x80)
  {
    ep = &pdev->dev.in_ep[ep_addr & 0x7F];
  }
  else
  {
    ep = &pdev->dev.out_ep[ep_addr & 0x7F];
  }
  
  /* Here we check if the endpoint is single or double Buffer*/
  if (ep_kind == USB_SNG_BUF)
  {
    /*Single Buffer*/
    ep->doublebuffer = 0;
    /*Configure te PMA*/
    ep->pmaadress = (uint16_t)pmaadress;
  }
  else /*USB_DBL_BUF*/
  {
    /*Double Buffer Endpoint*/
    ep->doublebuffer = 1;
    /*Configure the PMA*/
    ep->pmaaddr0 =  pmaadress & 0xFFFF;
    ep->pmaaddr1 =  (pmaadress & 0xFFFF0000) >> 16;
  }
  
  return USB_OK; 
}

/**
  * @brief Configure an EP
  * @param  pdev : Device instance
  * @param  ep_addr: endpoint address
  * @param  ep_mps: endpoint max packet size
  * @param  ep_type: endpoint Type
  */
uint32_t DCD_EP_Open(USB_CORE_HANDLE *pdev , 
                     uint16_t ep_addr,
                     uint16_t ep_mps,
                     uint8_t ep_type)
{
  
  USB_EP *ep;
  
  /* initialize ep structure*/
  if ((ep_addr & 0x80) == 0x80)
  {
    ep = &pdev->dev.in_ep[ep_addr & 0x7F];
    ep->is_in = 1;
  }
  else
  {
    ep = &pdev->dev.out_ep[ep_addr & 0x7F];
    ep->is_in = 0;
  }
  
  ep->maxpacket = ep_mps;
  ep->type = ep_type;
  ep->num   = ep_addr & 0x7F;
  
  if (ep->num == 0)
  {
    /* Initialize the control transfer variables*/ 
    ep->ctl_data_len =0;
    ep->rem_data_len = 0;
    ep->total_data_len = 0;
  }
  
  /* Initialize the transaction level variables */
  ep->xfer_buff = 0;
  ep->xfer_len = 0;
  ep->xfer_count = 0;
  ep->is_stall = 0;
  
  /* initialize HW */
  switch (ep->type)
  {
  case MY_USB_EP_CONTROL:
    SetEPType(ep->num, EP_CONTROL);
    break;
  case MY_USB_EP_BULK:
    SetEPType(ep->num, EP_BULK);
    break;
  case MY_USB_EP_INT:
    SetEPType(ep->num, EP_INTERRUPT);
    break;
  case MY_USB_EP_ISOC:
    SetEPType(ep->num, EP_ISOCHRONOUS);
    break;
  } 
  
  if (ep->doublebuffer == 0) 
  {
    if (ep->is_in)
    {
      /*Set the endpoint Transmit buffer address */
      SetEPTxAddr(ep->num, ep->pmaadress);
      ClearDTOG_TX(ep->num);
      /* Configure NAK status for the Endpoint*/
      SetEPTxStatus(ep->num, EP_TX_NAK); 
    }
    else
    {
      /*Set the endpoint Receive buffer address */
      SetEPRxAddr(ep->num, ep->pmaadress);
      /*Set the endpoint Receive buffer counter*/
      SetEPRxCount(ep->num, ep->maxpacket);
      ClearDTOG_RX(ep->num);
      /* Configure VALID status for the Endpoint*/
      SetEPRxStatus(ep->num, EP_RX_VALID);
    }
  }
  /*Double Buffer*/
  else
  {
    /*Set the endpoint as double buffered*/
    SetEPDoubleBuff(ep->num);
    /*Set buffer address for double buffered mode*/
    SetEPDblBuffAddr(ep->num,ep->pmaaddr0, ep->pmaaddr1);
    
    if (ep->is_in==0)
    {
      /* Clear the data toggle bits for the endpoint IN/OUT*/
      ClearDTOG_RX(ep->num);
      ClearDTOG_TX(ep->num);
      
      /* Reset value of the data toggle bits for the endpoint out*/
      ToggleDTOG_TX(ep->num);
      
      SetEPRxStatus(ep->num, EP_RX_VALID);
      SetEPTxStatus(ep->num, EP_TX_DIS);
    }
    else
    {
      /* Clear the data toggle bits for the endpoint IN/OUT*/
      ClearDTOG_RX(ep->num);
      ClearDTOG_TX(ep->num);
      ToggleDTOG_RX(ep->num);
      /* Configure DISABLE status for the Endpoint*/
      SetEPTxStatus(ep->num, EP_TX_DIS);
      SetEPRxStatus(ep->num, EP_RX_DIS);
    }
  } 
  return USB_OK; 
}
/**
  * @brief called when an EP is disabled
  * @param  pdev: device instance
  * @param  ep_addr: endpoint address
  * @retval : status
  */
uint32_t DCD_EP_Close(USB_CORE_HANDLE *pdev , uint8_t  ep_addr)
{
  USB_EP *ep;
  
  if ((ep_addr&0x80) == 0x80)
  {
    ep = &pdev->dev.in_ep[ep_addr & 0x7F];
  }
  else
  {
    ep = &pdev->dev.out_ep[ep_addr & 0x7F];
  }
  
  if (ep->doublebuffer == 0) 
  {
    if (ep->is_in)
    {
      ClearDTOG_TX(ep->num);
      /* Configure DISABLE status for the Endpoint*/
      SetEPTxStatus(ep->num, EP_TX_DIS); 
    }
    else
    {
      ClearDTOG_RX(ep->num);
      /* Configure DISABLE status for the Endpoint*/
      SetEPRxStatus(ep->num, EP_RX_DIS);
    }
  }
  /*Double Buffer*/
  else
  { 
    if (ep->is_in==0)
    {
      /* Clear the data toggle bits for the endpoint IN/OUT*/
      ClearDTOG_RX(ep->num);
      ClearDTOG_TX(ep->num);
      
      /* Reset value of the data toggle bits for the endpoint out*/
      ToggleDTOG_TX(ep->num);
      
      SetEPRxStatus(ep->num, EP_RX_DIS);
      SetEPTxStatus(ep->num, EP_TX_DIS);
    }
    else
    {
      /* Clear the data toggle bits for the endpoint IN/OUT*/
      ClearDTOG_RX(ep->num);
      ClearDTOG_TX(ep->num);
      ToggleDTOG_RX(ep->num);
      /* Configure DISABLE status for the Endpoint*/
      SetEPTxStatus(ep->num, EP_TX_DIS);
      SetEPRxStatus(ep->num, EP_RX_DIS);
    }
  } 
  return USB_OK;
}


/**
  * @brief DCD_EP_PrepareRx
  * @param  pdev: device instance
  * @param  ep_addr: endpoint address
  * @param  pbuf: pointer to Rx buffer
  * @param  buf_len: data length
  * @retval : status
  */
uint32_t DCD_EP_PrepareRx( USB_CORE_HANDLE *pdev,
                          uint8_t   ep_addr,
                          uint8_t *pbuf,                        
                          uint16_t  buf_len)
{
  __IO uint32_t len = 0; 
  USB_EP *ep;
  
  ep = &pdev->dev.out_ep[ep_addr & 0x7F];
  
  /*setup and start the Xfer */
  ep->xfer_buff = pbuf;  
  ep->xfer_len = buf_len;
  ep->xfer_count = 0; 
  
  /*Multi packet transfer*/
  if (ep->xfer_len > ep->maxpacket)
  {
    len=ep->maxpacket;
    ep->xfer_len-=len; 
  }
  else
  {
    len=ep->xfer_len;
    ep->xfer_len =0;
  }
  
  /* configure and validate Rx endpoint */
  if (ep->doublebuffer == 0) 
  {
    /*Set RX buffer count*/
    SetEPRxCount(ep->num, len);
  }
  else
  {
    /*Set the Double buffer counter*/
    SetEPDblBuffCount(ep->num, ep->is_in, len);
  } 
  
  SetEPRxStatus(ep->num, EP_RX_VALID);
  
  return USB_OK;
}

/**
  * @brief Transmit data Buffer
  * @param  pdev: device instance
  * @param  ep_addr: endpoint address
  * @param  pbuf: pointer to Tx buffer
  * @param  buf_len: data length
  * @retval : status
  */
uint32_t  DCD_EP_Tx ( USB_CORE_HANDLE *pdev,
                     uint8_t   ep_addr,
                     uint8_t   *pbuf,
                     uint32_t   buf_len)
{
  __IO uint32_t len = 0; 
  USB_EP *ep;
  
  ep = &pdev->dev.in_ep[ep_addr & 0x7F];
  
  /*setup and start the Xfer */
  ep->num = ep_addr & 0x7F; 
  ep->xfer_buff = pbuf;  
  ep->xfer_len = buf_len;
  ep->xfer_count = 0; 
  
  /*Multi packet transfer*/
  if (ep->xfer_len > ep->maxpacket)
  {
    len=ep->maxpacket;
    ep->xfer_len-=len; 
  }
  else
  {
    len=ep->xfer_len;
    ep->xfer_len =0;
  }
  
  /* configure and validate Tx endpoint */
  if (ep->doublebuffer == 0) 
  {
    UserToPMABufferCopy(ep->xfer_buff, ep->pmaadress, len);
    SetEPTxCount(ep->num, len);
  }
  else
  {
    uint16_t pmabuffer=0;
    /*Set the Double buffer counter*/
    SetEPDblBuffCount(ep->num, ep->is_in, len);
    
    /*Write the data to the USB endpoint*/
    if (GetENDPOINT(ep->num)&EP_DTOG_TX)
    {
      pmabuffer = ep->pmaaddr1;
    }
    else
    {
      pmabuffer = ep->pmaaddr0;
    }
    UserToPMABufferCopy(ep->xfer_buff, pmabuffer, len);
    FreeUserBuffer(ep->num, ep->is_in);
  }
  
  SetEPTxStatus(ep->num, EP_TX_VALID);
  
  return USB_OK; 
}


/**
  * @brief Stall an endpoint.
  * @param  pdev: device instance
  * @param  epnum: endpoint address
  * @retval : status
  */
uint32_t  DCD_EP_Stall (USB_CORE_HANDLE *pdev, uint8_t   epnum)
{
  USB_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pdev->dev.in_ep[epnum & 0x7F];    
  }
  else
  {
    ep = &pdev->dev.out_ep[epnum];
  }
  
  if (ep->num ==0)
  {
    /* This macro sets STALL status for RX & TX*/ 
    _SetEPRxTxStatus(ep->num,EP_RX_STALL,EP_TX_STALL); 
    /*Endpoint is stalled */
    ep->is_stall = 1;
    return USB_OK;
  }
  if (ep->is_in)
  {  
    /* IN endpoint */
    ep->is_stall = 1;
    /* IN Endpoint stalled */
   SetEPTxStatus(ep->num , EP_TX_STALL); 
  }
  else
  { 
    ep->is_stall = 1;
    /* OUT Endpoint stalled */
    SetEPRxStatus(ep->num , EP_RX_STALL);
  }
  
  return USB_OK;
}


/**
  * @brief Clear stall condition on endpoints.
  * @param  pdev: device instance
  * @param  epnum: endpoint address
  * @retval : status
  */
uint32_t  DCD_EP_ClrStall (USB_CORE_HANDLE *pdev, uint8_t epnum)
{
  USB_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pdev->dev.in_ep[epnum & 0x7F];    
  }
  else
  {
    ep = &pdev->dev.out_ep[epnum];
  } 
  
  if (ep->is_in)
  {
    ClearDTOG_TX(ep->num);
    SetEPTxStatus(ep->num, EP_TX_VALID);
    ep->is_stall = 0;  
  }
  else
  {
    ClearDTOG_RX(ep->num);
    SetEPRxStatus(ep->num, EP_RX_VALID);
    ep->is_stall = 0;  
  }
  
  return USB_OK;
}

/**
  * @brief This Function set USB device address
  * @param  pdev: device instance
  * @param  address: new device address
  */
void  DCD_EP_SetAddress (USB_CORE_HANDLE *pdev, uint8_t address)
{
  uint32_t i=0;
  pdev->dev.device_address = address;
  
  /* set address in every used endpoint */
  for (i = 0; i < EP_NUM; i++)
  {
    _SetEPAddress((uint8_t)i, (uint8_t)i);
  } /* set device address and enable function */
  _SetDADDR(address | DADDR_EF); 
}

/**
  * @brief Connect device (enable internal pull-up)
  * @param  pdev: device instance
  * @retval : None
  */
void  DCD_DevConnect (USB_CORE_HANDLE *pdev)
{
  /* Enabling DP Pull-Down bit to Connect internal pull-up on USB DP line */
  *BCDR|=BCDR_DPPU;
  /*Device is in default state*/
  pdev->dev.device_status  = USB_DEFAULT;
}

/**
  * @brief Disconnect device (disable internal pull-up)
  * @param  pdev: device instance
  * @retval : None
  */
void  DCD_DevDisconnect (USB_CORE_HANDLE *pdev)
{
 
  /* Disable DP Pull-Down bit*/
  *BCDR&=~BCDR_DPPU;
  
  /*Device is in unconnected state*/
  pdev->dev.device_status  = USB_UNCONNECTED;
}

/**
  * @brief returns the EP Status
  * @param   pdev : Selected device
  *         epnum : endpoint address
  * @retval : EP status
  */

uint32_t DCD_GetEPStatus(USB_CORE_HANDLE *pdev ,uint8_t epnum)
{
  uint16_t Status=0; 
  
  USB_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pdev->dev.in_ep[epnum & 0x7F];    
  }
  else
  {
    ep = &pdev->dev.out_ep[epnum];
  } 
  
  if (ep->is_in)
  {
    Status = GetEPTxStatus(ep->num);
  }
  else
  {
    Status = GetEPRxStatus(ep->num);
  }
  
  return Status; 
}

/**
  * @brief Set the EP Status
  * @param   pdev : Selected device
  *         Status : new Status
  *         epnum : EP address
  * @retval : None
  */
void DCD_SetEPStatus (USB_CORE_HANDLE *pdev , uint8_t epnum , uint32_t Status)
{
  USB_EP *ep;
  if ((0x80 & epnum) == 0x80)
  {
    ep = &pdev->dev.in_ep[epnum & 0x7F];    
  }
  else
  {
    ep = &pdev->dev.out_ep[epnum];
  } 
  
  if (ep->is_in)
  {
    SetEPTxStatus(ep->num, (uint16_t)Status);
  }
  else
  {
    SetEPRxStatus(ep->num, (uint16_t)Status);
  }
    
  if ((Status == EP_RX_STALL) || (Status == EP_TX_STALL))
  {
    ep->is_stall =1;
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
