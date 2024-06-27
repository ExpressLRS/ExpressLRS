/**
  ******************************************************************************
  * @file    usb_dcd.h
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    31-January-2014
  * @brief   Device Control Driver Header file
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
#ifndef __DCD_H__
#define __DCD_H__

/* Includes ------------------------------------------------------------------*/
#include "usb_core.h"

/* Exported defines ----------------------------------------------------------*/
#define MY_USB_EP_CONTROL                       0
#define MY_USB_EP_ISOC                          1
#define MY_USB_EP_BULK                          2
#define MY_USB_EP_INT                           3

/*  Endpoint Kind */
#define USB_SNG_BUF                          0
#define USB_DBL_BUF                          1

/*  Device Status */
#define USB_UNCONNECTED                      0
#define USB_DEFAULT                          1
#define USB_ADDRESSED                        2
#define USB_CONFIGURED                       3
#define USB_SUSPENDED                        4


/* Exported types ------------------------------------------------------------*/
/********************************************************************************
Data structure type
********************************************************************************/
typedef struct
{
  uint8_t  bLength;
  uint8_t  bDescriptorType;
  uint8_t  bEndpointAddress;
  uint8_t  bmAttributes;
  uint16_t wMaxPacketSize;
  uint8_t  bInterval;
}
EP_DESCRIPTOR , *PEP_DESCRIPTOR;

typedef struct USB_ep
{
  uint8_t        num;
  uint8_t        is_in;
  uint8_t        is_stall;  
  uint8_t        type;
  uint16_t       pmaadress;
  uint16_t       pmaaddr0;
  uint16_t       pmaaddr1;
  uint8_t        doublebuffer;      
  uint32_t       maxpacket;
  /* transaction level variables !!! up to one max packet per transaction !!! */
  uint8_t        *xfer_buff;
  uint32_t       xfer_len ;
  uint32_t       xfer_count;
  /* control transfer variables*/  
  uint32_t       rem_data_len;
  uint32_t       total_data_len;
  uint32_t       ctl_data_len;  
}
USB_EP;

typedef  struct  usb_setup_req {
    
    uint8_t   bmRequest;                      
    uint8_t   bRequest;                           
    uint16_t  wValue;                             
    uint16_t  wIndex;                             
    uint16_t  wLength;                            
} USB_SETUP_REQ;

typedef struct _Device_TypeDef
{
  uint8_t  *(*GetDeviceDescriptor)( uint8_t speed , uint16_t *length);  
#ifdef LPM_ENABLED
  uint8_t  *(*GetBOSDescriptor)( uint8_t speed , uint16_t *length);
#endif
  uint8_t  *(*GetLangIDStrDescriptor)( uint8_t speed , uint16_t *length); 
  uint8_t  *(*GetManufacturerStrDescriptor)( uint8_t speed , uint16_t *length);  
  uint8_t  *(*GetProductStrDescriptor)( uint8_t speed , uint16_t *length);  
  uint8_t  *(*GetSerialStrDescriptor)( uint8_t speed , uint16_t *length);  
  uint8_t  *(*GetConfigurationStrDescriptor)( uint8_t speed , uint16_t *length);  
  uint8_t  *(*GetInterfaceStrDescriptor)( uint8_t speed , uint16_t *length);   
} USBD_DEVICE, *pUSBD_DEVICE;




typedef struct _Device_cb
{
  uint8_t  (*Init)         (void *pdev , uint8_t cfgidx);
  uint8_t  (*DeInit)       (void *pdev , uint8_t cfgidx);
 /* Control Endpoints*/
  uint8_t  (*Setup)        (void *pdev , USB_SETUP_REQ  *req);  
  uint8_t  (*EP0_TxSent)   (void *pdev );    
  uint8_t  (*EP0_RxReady)  (void *pdev);  
  /* Class Specific Endpoints*/
  uint8_t  (*DataIn)       (void *pdev , uint8_t epnum);   
  uint8_t  (*DataOut)      (void *pdev , uint8_t epnum); 
  uint8_t  (*SOF)          (void *pdev);   
  uint8_t  *(*GetConfigDescriptor)( uint8_t speed , uint16_t *length); 

#ifdef USB_SUPPORT_USER_STRING_DESC 
  uint8_t  *(*GetUsrStrDescriptor)( uint8_t speed ,uint8_t index,  uint16_t *length);   
#endif  
  
} USBD_Class_cb_TypeDef;



typedef struct _USBD_USR_PROP
{
  void (*Init)(void);   
  void (*DeviceReset)(uint8_t speed); 
  void (*DeviceConfigured)(void);
  void (*DeviceSuspended)(void);
  void (*DeviceResumed)(void);      
}
USBD_Usr_cb_TypeDef;

typedef struct _DCD
{
  uint8_t        device_config;
  uint8_t        device_state;
  uint8_t        device_status;
  uint8_t        device_old_status;
  uint8_t        device_address;
  uint32_t       DevRemoteWakeup;
  uint32_t       speed;
  uint8_t        setup_packet [8];
  USB_EP     in_ep   [EP_NUM];
  USB_EP     out_ep  [EP_NUM];
  USBD_Class_cb_TypeDef         *class_cb;
  USBD_Usr_cb_TypeDef           *usr_cb;
  USBD_DEVICE                   *usr_device;  
  uint8_t        *pConfig_descriptor;
 }
DCD_DEV , *DCD_PDEV;

typedef struct USB_Device_handle
{
  DCD_DEV     dev;
}
USB_DEVICE_HANDLE, USB_CORE_HANDLE;

/* Exported macros -----------------------------------------------------------*/
/* Exported variables --------------------------------------------------------*/
/* Exported functions ------------------------------------------------------- */ 

/********************************************************************************
EXPORTED FUNCTION FROM THE USB-DEVICE LAYER
********************************************************************************/
void       DCD_Init(USB_CORE_HANDLE *pdev);

void        DCD_DevConnect (USB_CORE_HANDLE *pdev);
void        DCD_DevDisconnect (USB_CORE_HANDLE *pdev);
void        DCD_EP_SetAddress (USB_CORE_HANDLE *pdev,
                               uint8_t address);

uint32_t DCD_PMA_Config(USB_CORE_HANDLE *pdev , 
                        uint16_t ep_addr,
                        uint16_t ep_kind,
                        uint32_t pmaadress);

uint32_t    DCD_EP_Open(USB_CORE_HANDLE *pdev , 
                     uint16_t ep_addr,
                     uint16_t ep_mps,
                     uint8_t ep_type);

uint32_t    DCD_EP_Close  (USB_CORE_HANDLE *pdev,
                                uint8_t  ep_addr);


uint32_t   DCD_EP_PrepareRx ( USB_CORE_HANDLE *pdev,
                        uint8_t   ep_addr,                                  
                        uint8_t *pbuf,                                  
                        uint16_t  buf_len);
  
uint32_t    DCD_EP_Tx (USB_CORE_HANDLE *pdev,
                               uint8_t  ep_addr,
                               uint8_t  *pbuf,
                               uint32_t   buf_len);
uint32_t    DCD_EP_Stall (USB_CORE_HANDLE *pdev,
                              uint8_t   epnum);
uint32_t    DCD_EP_ClrStall (USB_CORE_HANDLE *pdev,
                                  uint8_t epnum);

uint32_t    DCD_Handle_ISR(USB_CORE_HANDLE *pdev);

uint32_t DCD_GetEPStatus(USB_CORE_HANDLE *pdev ,
                         uint8_t epnum);

void DCD_SetEPStatus (USB_CORE_HANDLE *pdev , 
                      uint8_t epnum , 
                      uint32_t Status);

void DCD_StopDevice(USB_CORE_HANDLE *pdev);

#endif /* __DCD_H__ */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
