/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include "usbd_upd_hid_core.h"
#include "SystemCommon.h"
#include "usbd_usr.h"
#include "wiring_time.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/ 
uint8_t  USBD_HID_Init (void  *pdev, 
                               uint8_t cfgidx);

uint8_t  USBD_HID_DeInit (void  *pdev, 
                                 uint8_t cfgidx);

uint8_t  USBD_HID_Setup (void  *pdev, 
                                USB_SETUP_REQ *req);

uint8_t  *USBD_HID_GetCfgDesc (uint8_t speed, uint16_t *length);


uint8_t  USBD_HID_DataIn (void  *pdev, uint8_t epnum);


uint8_t  USBD_HID_DataOut (void  *pdev, uint8_t epnum);


uint8_t  USBD_HID_EP0_RxReady (void  *pdev);

USBD_Class_cb_TypeDef  USBD_HID_cb = 
{
  USBD_HID_Init,
  USBD_HID_DeInit,
  USBD_HID_Setup,
  NULL, /*EP0_TxSent*/  
  USBD_HID_EP0_RxReady, /*EP0_RxReady*/ /* STATUS STAGE IN */
  USBD_HID_DataIn, /*DataIn*/
  USBD_HID_DataOut, /*DataOut*/
  NULL, /*SOF */    
  USBD_HID_GetCfgDesc, 
};
  
sUPD_Info UPDInfo;
sUPD_SoftwareInfo SoftwareInfo;
uint8_t Report_buf[USB_REPORT_BUFF_SIZE];
uint8_t USBD_HID_Report_ID=0;
uint8_t flag = 0;
uint8_t PrevXferDone;
sUSBC_HID_UPD_Report USBC_HID_UPD_ReportOut __attribute__((aligned(4)));
sUSBC_HID_UPD_Report USBC_HID_UPD_ReportIn __attribute__((aligned(4)));

static uint32_t  USBD_HID_AltSet = 0;
    
static uint32_t  USBD_HID_Protocol = 0;
 
static uint32_t  USBD_HID_IdleState = 0;

/* USB HID device Configuration Descriptor */
const uint8_t USBD_HID_CfgDesc[CUSTOMHID_SIZ_CONFIG_DESC] =
{
	0x09, /* bLength: Configuration Descriptor size */
  USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType: Configuration */
  CUSTOMHID_SIZ_CONFIG_DESC,
  /* wTotalLength: Bytes returned */
  0x00,
  0x01,         /*bNumInterfaces: 1 interface*/
  0x01,         /*bConfigurationValue: Configuration value*/
  0x00,         /*iConfiguration: Index of string descriptor describing
  the configuration*/
  0x80,//0xC0,         /*bmAttributes: bus powered and Support Remote Wake-up */
  250,//0x32,         /*MaxPower 100 mA: this current is used for detecting Vbus*/
  
  /************** Descriptor of Custom HID interface ****************/
  /* 09 */
  0x09,         /*bLength: Interface Descriptor size*/
  USB_INTERFACE_DESCRIPTOR_TYPE,/*bDescriptorType: Interface descriptor type*/
  0x00,         /*bInterfaceNumber: Number of Interface*/
  0x00,         /*bAlternateSetting: Alternate setting*/
  0x02,         /*bNumEndpoints*/
  0x03,         /*bInterfaceClass: HID*/
  0x00,         /*bInterfaceSubClass : 1=BOOT, 0=no boot*/
  0x00,         /*nInterfaceProtocol : 0=none, 1=keyboard, 2=mouse*/
  0,            /*iInterface: Index of string descriptor*/
  /******************** Descriptor of Custom HID ********************/
  /* 18 */
  0x09,         /*bLength: HID Descriptor size*/
  HID_DESCRIPTOR_TYPE, /*bDescriptorType: HID*/
  0x11,         /*bcdHID: HID Class Spec release number*/
  0x01,
  0x00,         /*bCountryCode: Hardware target country*/
  0x01,         /*bNumDescriptors: Number of HID class descriptors to follow*/
  0x22,         /*bDescriptorType*/
  CUSTOMHID_SIZ_REPORT_DESC,/*wItemLength: Total length of Report descriptor*/
  0x00,
  /******************** Descriptor of Custom HID endpoints ***********/
  /* 27 */
  0x07,          /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE, /* bDescriptorType: */
  
  HID_IN_EP,     /* bEndpointAddress: Endpoint Address (IN) */
  0x03,          /* bmAttributes: Interrupt endpoint */
  HID_IN_PACKET&0xff, /* wMaxPacketSize: 2 Bytes max */
  HID_IN_PACKET>>8,//0x00,
  0x04,//0x20,          /* bInterval: Polling Interval (32 ms) */
  /* 34 */
  
  0x07,	         /* bLength: Endpoint Descriptor size */
  USB_ENDPOINT_DESCRIPTOR_TYPE,	/* bDescriptorType: */
  /*	Endpoint descriptor type */
  HID_OUT_EP,	/* bEndpointAddress: */
  /*	Endpoint Address (OUT) */
  0x03,	/* bmAttributes: Interrupt endpoint */
  HID_OUT_PACKET&0xff,	/* wMaxPacketSize: 2 Bytes max  */
  HID_OUT_PACKET>>8,//0x00,
  0x01//0x20,	/* bInterval: Polling Interval (20 ms) */
  /* 41 */
} ;

const uint8_t CustomHID_ReportDescriptor[CUSTOMHID_SIZ_REPORT_DESC] =
{
	0x06, 0x00, 0xff,              // USAGE_PAGE (Vendor Defined Page 1)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x81,                    //   LOGICAL_MINIMUM (-127)
    0x25, 0x7f,                    //   LOGICAL_MAXIMUM (127)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x96, USB_HID_UPD_REPORT_SIZE&0xFF, USB_HID_UPD_REPORT_SIZE>>8, //   REPORT_COUNT (USB_HID_UPD_REPORT_SIZE)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x81, 0x02,                    //   INPUT (Data,Var,Abs)
    0x09, 0x01,                    //   USAGE (Vendor Usage 1)
    0x91, 0x02,                    //   OUTPUT (Data,Var,Abs)
    0xc0,                          // END_COLLECTION
}; /* CustomHID_ReportDescriptor */

/* Private function ----------------------------------------------------------*/ 
/**
  * @brief  USBD_HID_Init
  *         Initialize the HID interface
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
uint8_t  USBD_HID_Init (void  *pdev, 
                               uint8_t cfgidx)
{
  DCD_PMA_Config(pdev , HID_IN_EP,USB_SNG_BUF,HID_IN_TX_ADDRESS);
  DCD_PMA_Config(pdev , HID_OUT_EP,USB_SNG_BUF,HID_OUT_RX_ADDRESS);

  /* Open EP IN */
  DCD_EP_Open(pdev,
              HID_IN_EP,
              HID_IN_PACKET,
              MY_USB_EP_INT);
  
  /* Open EP OUT */
  DCD_EP_Open(pdev,
              HID_OUT_EP,
              HID_OUT_PACKET,
              MY_USB_EP_INT);
 
  /*Receive Data*/
  DCD_EP_PrepareRx(pdev,HID_OUT_EP,Report_buf,USB_REPORT_BUFF_SIZE);
  
  return USBD_OK;
}

/**
  * @brief  USBD_HID_Init
  *         DeInitialize the HID layer
  * @param  pdev: device instance
  * @param  cfgidx: Configuration index
  * @retval status
  */
uint8_t  USBD_HID_DeInit (void  *pdev, 
                                 uint8_t cfgidx)
{
  /* Close HID EPs */
  DCD_EP_Close (pdev , HID_IN_EP);
  DCD_EP_Close (pdev , HID_OUT_EP);
  
  return USBD_OK;
}

/**
  * @brief  USBD_HID_Setup
  *         Handle the HID specific requests
  * @param  pdev: instance
  * @param  req: usb requests
  * @retval status
  */
uint8_t  USBD_HID_Setup (void  *pdev, 
                                USB_SETUP_REQ *req)
{
  uint8_t USBD_HID_Report_LENGTH=0;
  uint16_t len = 0;
  uint8_t  *pbuf = NULL;

  
  switch (req->bmRequest & USB_REQ_TYPE_MASK)
  {
  case USB_REQ_TYPE_CLASS :  
    switch (req->bRequest)
    {
    case HID_REQ_SET_PROTOCOL:
      USBD_HID_Protocol = (uint8_t)(req->wValue);
      break;
      
    case HID_REQ_GET_PROTOCOL:
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&USBD_HID_Protocol,
                        1);    
      break;
      
    case HID_REQ_SET_IDLE:
      USBD_HID_IdleState = (uint8_t)(req->wValue >> 8);
      break;
      
    case HID_REQ_GET_IDLE:
      USBD_CtlSendData (pdev, 
                        (uint8_t *)&USBD_HID_IdleState,
                        1);        
      break;
      
    case HID_REQ_SET_REPORT:
      flag = 1;
      USBD_HID_Report_ID = (uint8_t)(req->wValue);
      USBD_HID_Report_LENGTH = (uint8_t)(req->wLength);
      USBD_CtlPrepareRx (pdev, Report_buf, USBD_HID_Report_LENGTH);
      
      break;
   
    default:
      USBD_CtlError (pdev, req);
      return USBD_FAIL; 
    }
    break;
    
  case USB_REQ_TYPE_STANDARD:
    switch (req->bRequest)
    {
    case USB_REQ_GET_DESCRIPTOR: 
      if( req->wValue >> 8 == HID_REPORT_DESC)
      {
        len = MIN(CUSTOMHID_SIZ_REPORT_DESC , req->wLength);
        pbuf = (uint8_t*)CustomHID_ReportDescriptor;
      }
      else if( req->wValue >> 8 == HID_DESCRIPTOR_TYPE)
      {
        pbuf = (uint8_t*)USBD_HID_CfgDesc + 0x12;
        len = MIN(USB_HID_DESC_SIZ , req->wLength);
      }
      
      USBD_CtlSendData (pdev, 
                        pbuf,
                        len);
      
      break;
      
    case USB_REQ_GET_INTERFACE :
      USBD_CtlSendData (pdev,
                        (uint8_t *)&USBD_HID_AltSet,
                        1);
      break;
      
    case USB_REQ_SET_INTERFACE :
      USBD_HID_AltSet = (uint8_t)(req->wValue);
      break;
    }
  }
  return USBD_OK;
}

/**
  * @brief  USBD_HID_SendReport 
  *         Send HID Report
  * @param  pdev: device instance
  * @param  buff: pointer to report
  * @retval status
  */
uint8_t USBD_HID_SendReport     (USB_CORE_HANDLE  *pdev, 
                                 uint8_t *report,
                                 uint16_t len)
{
  /* Check if USB is configured */
  if (pdev->dev.device_status == USB_CONFIGURED )
  {
    DCD_EP_Tx (pdev, HID_IN_EP, report, len);
  }
  return USBD_OK;
}

/**
  * @brief  USBD_HID_GetCfgDesc 
  *         return configuration descriptor
  * @param  speed : current device speed
  * @param  length : pointer data length
  * @retval pointer to descriptor buffer
  */
uint8_t  *USBD_HID_GetCfgDesc (uint8_t speed, uint16_t *length)
{
  *length = sizeof (USBD_HID_CfgDesc);
  return (uint8_t*)USBD_HID_CfgDesc;
}

/**
  * @brief  USBD_HID_DataIn
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
uint8_t  USBD_HID_DataIn (void  *pdev, 
                                 uint8_t epnum)
{
  if (epnum == 1) PrevXferDone = 1;

  return USBD_OK;
}

/**
  * @brief  USBD_HID_DataOut
  *         handle data IN Stage
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */
uint8_t  USBD_HID_DataOut (void  *pdev, 
                                  uint8_t epnum)
{ 
  DCD_EP_PrepareRx(pdev,HID_IN_EP,Report_buf,USB_REPORT_BUFF_SIZE);
	memcpy(&USBC_HID_UPD_ReportOut,Report_buf,sizeof(USBC_HID_UPD_ReportOut));
  return USBD_OK;
}

/**
  * @brief  USBD_HID_EP0_RxReady
  *         Handles control request data.
  * @param  pdev: device instance
  * @param  epnum: endpoint index
  * @retval status
  */

uint8_t USBD_HID_EP0_RxReady(void *pdev)
{  
  if (flag == 1)
  {
    flag = 0;    
  }
  return USBD_OK;
}

USB_CORE_HANDLE  USB_Device_dev ;
volatile unsigned long USB_SysTickMs=0;
unsigned long USBC_HID_UPD_LastCommandNb;

void USB_IRQHandler(void)
{
	USB_Istr();
	return;
}

void USB_Init(void)
{
	USBD_Init(&USB_Device_dev,
			  &USR_desc, 
			  &USBD_HID_cb, 
			  &USR_cb);
	return;
}

void USB_UPD_Background(void)
{
	//	Report_buf
	sUSBC_HID_UPD_Report Report;
	unsigned long SYS_SysTickMs = millis();
	if(SYS_SysTickMs-USB_SysTickMs<=5)
		return;	
	USB_SysTickMs=SYS_SysTickMs;
	Report.CommandNb=USBC_HID_UPD_ReportOut.CommandNb;	
	if (Report.CommandNb==USBC_HID_UPD_LastCommandNb)
	{
		return;
	}		
	memcpy(&Report,&USBC_HID_UPD_ReportOut,sizeof(sUSBC_HID_UPD_Report));
	USBC_HID_UPD_LastCommandNb=Report.CommandNb;
		
	// Init USB ReportIn
	USBC_HID_UPD_ReportIn.CommandNb	 = Report.CommandNb;
	USBC_HID_UPD_ReportIn.CommandCode= Report.CommandCode;
	USBC_HID_UPD_ReportIn.Parameter1 = Report.Parameter1;
	USBC_HID_UPD_ReportIn.Parameter2 = Report.Parameter2;
	USBC_HID_UPD_ReportIn.Parameter3 = Report.Parameter3;
	USBC_HID_UPD_ReportIn.Parameter4 = Report.Parameter4;
	
	// Command search and details
	switch(Report.CommandCode)
	{
		case UPDCC_GET_INFO:
		{				
			UPDInfo.HardwareVersionL=*HardwareVersionL;
			UPDInfo.HardwareVersionH=*HardwareVersionH;
			UPDInfo.FirmwareVersionL=FirmwareVersionL;
			UPDInfo.FirmwareVersionH=FirmwareVersionH;			
			UPDInfo.BootloaderVersion=*BootloaderVersion;			
			UPDInfo.SDRAMSize=0;
			UPDInfo.FlashSize=MAX_FIRMWARE_SIZE;
			UPDInfo.FlashSectorNb=0;
			UPDInfo.SPIFlashSize=0;
			UPDInfo.SPIFlashPageSize=0;
			UPDInfo.SPIFlashSectorSize=0;
			UPDInfo.SPIFlashBlockSize=0;					
			memcpy(USBC_HID_UPD_ReportIn.Data,&UPDInfo,sizeof(sUPD_Info));
				
			break;
		}	
		case UPDCC_SET_TX_STATE:
		{
			if( Report.Parameter1==0 && 
				Report.Parameter2==2   )
			{
                SYS_EnterUpdate();
			}
			break;
		}	
		case UPDCC_GET_RXTX_STATE:
		{
			
            USBC_HID_UPD_ReportIn.Parameter1 = 0;
            USBC_HID_UPD_ReportIn.Parameter2 = 0;			
			
			break;
		}		
		case UPDCC_GET_UID:
		{
			uint32_t UID[3];
            char TMPBuf[32]; 			
			
			UID[0]=*((uint32_t *)UID_BASE);
			UID[1]=*((uint32_t *)UID_BASE+1);
			UID[2]=*((uint32_t *)UID_BASE+2);
			sprintf( TMPBuf, "%08X%08X%08X", UID[2], UID[1], UID[0] );
            memcpy( USBC_HID_UPD_ReportIn.Data, TMPBuf, 24 );			
			break;
		}		
		case UPDCC_GET_SET_RX_INFO:
		{
			// GET
			if(Report.Parameter1==0)
			{
				uint32_t *pData = (uint32_t *)USBC_HID_UPD_ReportIn.Data; 				
								
                pData[0] = 0xFFFFFFFF;
                pData[1] = 0xFFFFFFFF;
                pData[2] = 0xFFFFFFFF;
                pData[3] = 0xFFFFFFFF;
                pData[4] = 0xFFFFFFFF;				
			}
			break;
		}	
		case UPDCC_GET_TX_INFO:
		{
			if( Report.Parameter1==0 )
			{
				uint32_t *pData = (uint32_t *)USBC_HID_UPD_ReportIn.Data; 
				
				pData[0] = *ProductNumber;
				pData[1] = FirmwareVersionL;
				pData[2] = RFLibraryVersion;//FirmwareVersionL;
				pData[3] = *RFM_TXID;
			}
			break;
		}		
		case UPDCC_GET_RX_UPDATE_STATE:
		{
			if( Report.Parameter1 == 0 )
			{
        USBC_HID_UPD_ReportIn.Parameter2=4;
        USBC_HID_UPD_ReportIn.Parameter3=0;     			
			}
			break;
		}
		case UPDCC_ERASE_RFM:
		{
      SYS_EnterUpdate();
			break;
		}
		case UPDCC_READ_SOFTWARE_INFO:
		{
			SoftwareInfo.SoftwareVersionH=FirmwareVersionH;
			SoftwareInfo.SoftwareVersionL=FirmwareVersionL;
			memcpy(USBC_HID_UPD_ReportIn.Data,(void*)&SoftwareInfo,sizeof(sUPD_SoftwareInfo));
			break;
		}
		default:
			break;
	}
	// Send report
	USBD_HID_SendReport(&USB_Device_dev,(uint8_t*)&USBC_HID_UPD_ReportIn,sizeof(USBC_HID_UPD_ReportIn));
}

void SYS_EnterUpdate(void)
{	
	ResetSignature[0]=RESET_SIGNATURE_1;
	ResetSignature[1]=RESET_SIGNATURE_2;
	NVIC_SystemReset();		
	while(1);
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
