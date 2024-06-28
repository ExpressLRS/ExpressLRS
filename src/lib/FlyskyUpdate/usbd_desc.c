/**
  ******************************************************************************
  * @file    usbd_desc.c
  * @author  MCD Application Team
  * @version V1.0.0
  * @date    31-January-2014
  * @brief   This file provides the USBD descriptors and string formating method.
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
#include "usbd_desc.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define USBD_VID                     0x284e
#define USBD_PID                     FLYSKY_USBD_PID

#define USBD_LANGID_STRING            0x409
#define USBD_MANUFACTURER_STRING      "FlySky"

#define USBD_PRODUCT_FS_STRING        "TX firmware update"

#define USBD_CONFIGURATION_FS_STRING  "HID Config"
#define USBD_INTERFACE_FS_STRING      "HID Interface"

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
char USBD_SERIALNUMBER_FS_STRING[26];

USBD_DEVICE USR_desc =
{
  USBD_USR_DeviceDescriptor,
  USBD_USR_LangIDStrDescriptor, 
  USBD_USR_ManufacturerStrDescriptor,
  USBD_USR_ProductStrDescriptor,
  USBD_USR_SerialStrDescriptor,
  USBD_USR_ConfigStrDescriptor,
  USBD_USR_InterfaceStrDescriptor, 
};

/* USB Standard Device Descriptor */
const uint8_t USBD_DeviceDesc[USB_SIZ_DEVICE_DESC] =
{
  0x12,                       /*bLength */
  USB_DEVICE_DESCRIPTOR_TYPE, /*bDescriptorType*/
  0x00,                       /*bcdUSB */
  0x02,
  0x00,                       /*bDeviceClass*/
  0x00,                       /*bDeviceSubClass*/
  0x00,                       /*bDeviceProtocol*/
  USB_MAX_EP0_SIZE,           /*bMaxPacketSize*/
  LOBYTE(USBD_VID),           /*idVendor*/
  HIBYTE(USBD_VID),           /*idVendor*/
  LOBYTE(USBD_PID),           /*idVendor*/
  HIBYTE(USBD_PID),           /*idVendor*/
  0x00,                       /*bcdDevice rel. 2.00*/
  0x01,//0x02,
  USBD_IDX_MFC_STR,           /*Index of manufacturer  string*/
  USBD_IDX_PRODUCT_STR,       /*Index of product string*/
  USBD_IDX_SERIAL_STR,        /*Index of serial number string*/
  USBD_CFG_MAX_NUM            /*bNumConfigurations*/
} ; /* USB_DeviceDescriptor */

/* USB Standard Device Descriptor */
const uint8_t USBD_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] =
{
  USB_LEN_DEV_QUALIFIER_DESC,
  USB_DESC_TYPE_DEVICE_QUALIFIER,
  0x00,
  0x02,
  0x00,
  0x00,
  0x00,
  0x40,
  0x01,
  0x00,
};

/* USB Standard Device Descriptor */
const uint8_t USBD_LangIDDesc[USB_SIZ_STRING_LANGID] =
{
  USB_SIZ_STRING_LANGID,         
  USB_DESC_TYPE_STRING,       
  LOBYTE(USBD_LANGID_STRING),
  HIBYTE(USBD_LANGID_STRING), 
};

uint8_t USBD_StringSerial[USB_SIZ_STRING_SERIAL] =
{
  USB_SIZ_STRING_SERIAL,       /* bLength */
  USB_STRING_DESCRIPTOR_TYPE,  /* bDescriptorType */
};

/* Private function prototypes -----------------------------------------------*/
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len);

/* Private functions ---------------------------------------------------------*/

/**
  * @brief return the device descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_DeviceDescriptor( uint8_t speed , uint16_t *length)
{
  *length = sizeof(USBD_DeviceDesc);
  return (uint8_t*)USBD_DeviceDesc;
}

/**
  * @brief  return the LangID string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_LangIDStrDescriptor( uint8_t speed , uint16_t *length)
{
  *length =  sizeof(USBD_LangIDDesc);  
  return (uint8_t*)USBD_LangIDDesc;
}

/**
  * @brief  return the product string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_ProductStrDescriptor( uint8_t speed , uint16_t *length)
{
  USBD_GetString ( (uint8_t*)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);    
  return USBD_StrDesc;
}

/**
  * @brief  return the manufacturer string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_ManufacturerStrDescriptor( uint8_t speed , uint16_t *length)
{
  USBD_GetString ( (uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
  return USBD_StrDesc;
}

/**
  * @brief  return the serial number string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_SerialStrDescriptor( uint8_t speed , uint16_t *length)
{
  *length = USB_SIZ_STRING_SERIAL; 
  return USBD_StringSerial;
}

/**
  * @brief return the configuration string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_ConfigStrDescriptor( uint8_t speed , uint16_t *length)
{
  USBD_GetString ( (uint8_t*)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc, length); 
  return USBD_StrDesc;  
}


/**
  * @brief  return the interface string descriptor
  * @param  speed : current device speed
  * @param  length : pointer to data length variable
  * @retval pointer to descriptor buffer
  */
uint8_t *  USBD_USR_InterfaceStrDescriptor( uint8_t speed , uint16_t *length)
{
  USBD_GetString ( (uint8_t*)USBD_INTERFACE_FS_STRING, USBD_StrDesc, length);
  return USBD_StrDesc;  
}

/**
  * @brief  Create the serial number string descriptor 
  * @param  None 
  * @retval None
  */
void Get_SerialNum(void)
{
  uint32_t Device_Serial0, Device_Serial1, Device_Serial2;
  
  Device_Serial0 = 0xFA181746;//*(uint32_t*)Device1_Identifier;
  Device_Serial1 = 0x13849759;//*(uint32_t*)Device2_Identifier;
  Device_Serial2 = 0xACD54585;//*(uint32_t*)Device3_Identifier;
  
  Device_Serial0 += Device_Serial2;
  
  if (Device_Serial0 != 0)
  {
    IntToUnicode (Device_Serial0, &USBD_StringSerial[2] ,8);
    IntToUnicode (Device_Serial1, &USBD_StringSerial[18] ,4);
  }
}

/**
  * @brief  Convert Hex 32Bits value into char 
  * @param  value: value to convert
  * @param  pbuf: pointer to the buffer 
  * @param  len: buffer length
  * @retval None
  */
static void IntToUnicode (uint32_t value , uint8_t *pbuf , uint8_t len)
{
  uint8_t idx = 0;
  
  for( idx = 0 ; idx < len ; idx ++)
  {
    if( ((value >> 28)) < 0xA )
    {
      pbuf[ 2* idx] = (value >> 28) + '0';
    }
    else
    {
      pbuf[2* idx] = (value >> 28) + 'A' - 10; 
    }
    
    value = value << 4;
    
    pbuf[ 2* idx + 1] = 0;
  }
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
