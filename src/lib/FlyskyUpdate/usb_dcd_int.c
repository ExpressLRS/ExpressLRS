/**
  ******************************************************************************
  * @file    usb_dcd_int.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   This file provides the interrupt subroutines for the Device
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
#include "usb_dcd_int.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern USB_CORE_HANDLE  USB_Device_dev;
extern uint32_t wInterrupt_Mask;
#ifdef LPM_ENABLED
__IO uint32_t  L1_remote_wakeup =0;
__IO uint32_t BESL = 0;
#endif

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Correct Transfer interrupt's service
  * @param  None
  * @retval None
  */
void CTR(void)
{
  USB_EP *ep;
  uint16_t count=0;
  uint8_t EPindex;
  __IO uint16_t wIstr;  
  __IO uint16_t wEPVal = 0;
  /* stay in loop while pending interrupts */
  while (((wIstr = _GetISTR()) & ISTR_CTR) != 0)
  {
    /* extract highest priority endpoint number */
    EPindex = (uint8_t)(wIstr & ISTR_EP_ID);
    
    if (EPindex == 0)
    {
      /* Decode and service control endpoint interrupt */
      
      /* DIR bit = origin of the interrupt */   
      if ((wIstr & ISTR_DIR) == 0)
      {
        /* DIR = 0 */
        
        /* DIR = 0      => IN  int */
        /* DIR = 0 implies that (EP_CTR_TX = 1) always  */
        _ClearEP_CTR_TX(ENDP0);
        ep = &((&USB_Device_dev)->dev.in_ep[0]);
        
        ep->xfer_count = GetEPTxCount(ep->num);
        ep->xfer_buff += ep->xfer_count;
 
        /* TX COMPLETE */
        USBD_DCD_INT_fops->DataInStage(&USB_Device_dev, 0x00);
      }
      else
      {
        /* DIR = 1 */
        
        /* DIR = 1 & CTR_RX       => SETUP or OUT int */
        /* DIR = 1 & (CTR_TX | CTR_RX) => 2 int pending */
        ep = &((&USB_Device_dev)->dev.out_ep[0]);
        wEPVal = _GetENDPOINT(ENDP0);
        
        if ((wEPVal &EP_SETUP) != 0)
        {
          /* Get SETUP Packet*/
          ep->xfer_count = GetEPRxCount(ep->num);
          PMAToUserBufferCopy(&((&USB_Device_dev)->dev.setup_packet[0]),ep->pmaadress , ep->xfer_count);       
          /* SETUP bit kept frozen while CTR_RX = 1*/ 
          _ClearEP_CTR_RX(ENDP0); 
          
          /* Process SETUP Packet*/
          USBD_DCD_INT_fops->SetupStage(&USB_Device_dev);
        }
        
        else if ((wEPVal & EP_CTR_RX) != 0)
        {
          _ClearEP_CTR_RX(ENDP0);
          /* Get Control Data OUT Packet*/
          ep->xfer_count = GetEPRxCount(ep->num);
          
          if (ep->xfer_count != 0)
          {
            PMAToUserBufferCopy(ep->xfer_buff, ep->pmaadress, ep->xfer_count);
            ep->xfer_buff+=ep->xfer_count;
          }
          
          /* Process Control Data OUT Packet*/
          USBD_DCD_INT_fops->DataOutStage(&USB_Device_dev, 0x00);
          
          _SetEPRxCount(ENDP0, ep->maxpacket);
          _SetEPRxStatus(ENDP0,EP_RX_VALID);
        }
      }
    }/* if(EPindex == 0) */
    else
    {
      
      /* Decode and service non control endpoints interrupt  */
      
      /* process related endpoint register */
      wEPVal = _GetENDPOINT(EPindex);
      if ((wEPVal & EP_CTR_RX) != 0)
      {  
        /* clear int flag */
        _ClearEP_CTR_RX(EPindex);
        ep = &((&USB_Device_dev)->dev.out_ep[EPindex]);
        
        /* OUT double Buffering*/
        if (ep->doublebuffer == 0)
        {
          count = GetEPRxCount(ep->num);
          if (count != 0)
          {
            PMAToUserBufferCopy(ep->xfer_buff, ep->pmaadress, count);
          }
        }
        else
        {
          if (GetENDPOINT(ep->num) & EP_DTOG_RX)
          {
            /*read from endpoint BUF0Addr buffer*/
            count = GetEPDblBuf0Count(ep->num);
            if (count != 0)
            {
              PMAToUserBufferCopy(ep->xfer_buff, ep->pmaaddr0, count);
            }
          }
          else
          {
            /*read from endpoint BUF1Addr buffer*/
            count = GetEPDblBuf1Count(ep->num);
            if (count != 0)
            {
              PMAToUserBufferCopy(ep->xfer_buff, ep->pmaaddr1, count);
            }
          }
          FreeUserBuffer(ep->num, EP_DBUF_OUT);  
        }
        /*multi-packet on the NON control OUT endpoint*/
        ep->xfer_count+=count;
        ep->xfer_buff+=count;
       
        if ((ep->xfer_len == 0) || (count < ep->maxpacket))
        {
          /* RX COMPLETE */
          USBD_DCD_INT_fops->DataOutStage(&USB_Device_dev, ep->num);
        }
        else
        {
          DCD_EP_PrepareRx (&USB_Device_dev,ep->num, ep->xfer_buff, ep->xfer_len);
        }
        
      } /* if((wEPVal & EP_CTR_RX) */
      
      if ((wEPVal & EP_CTR_TX) != 0)
      {
        ep = &((&USB_Device_dev)->dev.in_ep[EPindex]);
        
        /* clear int flag */
        _ClearEP_CTR_TX(EPindex);
        
        /* IN double Buffering*/
        if (ep->doublebuffer == 0)
        {
          ep->xfer_count = GetEPTxCount(ep->num);
          if (ep->xfer_count != 0)
          {
            UserToPMABufferCopy(ep->xfer_buff, ep->pmaadress, ep->xfer_count);
          }
        }
        else
        {
          if (GetENDPOINT(ep->num) & EP_DTOG_TX)
          {
            /*read from endpoint BUF0Addr buffer*/
            ep->xfer_count = GetEPDblBuf0Count(ep->num);
            if (ep->xfer_count != 0)
            {
              UserToPMABufferCopy(ep->xfer_buff, ep->pmaaddr0, ep->xfer_count);
            }
          }
          else
          {
            /*read from endpoint BUF1Addr buffer*/
            ep->xfer_count = GetEPDblBuf1Count(ep->num);
            if (ep->xfer_count != 0)
            {
              UserToPMABufferCopy(ep->xfer_buff, ep->pmaaddr1, ep->xfer_count);
            }
          }
          FreeUserBuffer(ep->num, EP_DBUF_IN);  
        }
        /*multi-packet on the NON control IN endpoint*/
        ep->xfer_count =GetEPTxCount(ep->num);
        ep->xfer_buff+=ep->xfer_count;
       
        /* Zero Length Packet? */
        if (ep->xfer_len == 0)
        {
          /* TX COMPLETE */
          USBD_DCD_INT_fops->DataInStage(&USB_Device_dev, ep->num);
        }
        else
        {
          DCD_EP_Tx  (&USB_Device_dev,ep->num, ep->xfer_buff, ep->xfer_len);
        }
        
      } /* if((wEPVal & EP_CTR_TX) != 0) */
      
    }/* if(EPindex == 0) else */
    
  }/* while(...) */
}

/**
  * @brief ISTR events interrupt service routine
  * @param  None
  * @retval None
  */
void USB_Istr(void)
{
  __IO uint16_t wIstr = 0; 
  
  wIstr = _GetISTR();
  
#if (IMR_MSK & ISTR_CTR)
  if (wIstr & ISTR_CTR & wInterrupt_Mask)
  {
    /* servicing of the endpoint correct transfer interrupt */
    /* clear of the CTR flag into the sub */
    CTR();
  }
#endif  
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_RESET)
  if (wIstr & ISTR_RESET & wInterrupt_Mask)
  {
    _SetISTR((uint16_t)CLR_RESET);
    USBD_DCD_INT_fops->Reset(&USB_Device_dev);
    DCD_EP_SetAddress(&USB_Device_dev, 0);
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_DOVR)
  if (wIstr & ISTR_DOVR & wInterrupt_Mask)
  {
    _SetISTR((uint16_t)CLR_DOVR);
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_ERR)
  if (wIstr & ISTR_ERR & wInterrupt_Mask)
  {
    _SetISTR((uint16_t)CLR_ERR);
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_WKUP)
  if (wIstr & ISTR_WKUP & wInterrupt_Mask)
  {
    _SetISTR((uint16_t)CLR_WKUP);
    
    USBD_DCD_INT_fops->Resume(&USB_Device_dev);
     
    /* Handle Resume state machine */  
    Resume(RESUME_EXTERNAL);

#ifdef LPM_ENABLED    
    /* clear L1 remote wakeup flag */
    L1_remote_wakeup = 0;
#endif
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_SUSP)
  if (wIstr & ISTR_SUSP & wInterrupt_Mask)
  {
   /* clear of the ISTR bit must be done after setting of CNTR_FSUSP */
    _SetISTR((uint16_t)CLR_SUSP);
    
    /* process library core layer suspend routine*/
    USBD_DCD_INT_fops->Suspend(&USB_Device_dev); 
    
    /* enter macrocell in suspend and system in low power mode when 
       USB_DEVICE_LOW_PWR_MGMT_SUPPORT defined in usb_conf.h */
    Suspend();   
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_SOF)
  if (wIstr & ISTR_SOF & wInterrupt_Mask)
  {
    _SetISTR((uint16_t)CLR_SOF);
    USBD_DCD_INT_fops->SOF(&USB_Device_dev);
  }
#endif
  /*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if (IMR_MSK & ISTR_ESOF)
  if (wIstr & ISTR_ESOF & wInterrupt_Mask)
  {
    /* clear ESOF flag in ISTR */
    _SetISTR((uint16_t)CLR_ESOF);
    
    /* resume handling timing is made with ESOFs */
    Resume(RESUME_ESOF); /* request without change of the machine state */
  }
#endif

} /* USB_Istr */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
