#if !defined(USB_UPDATE_H)
#define USB_UPDATE_H

#define    REPORT_DATA_SIZE          (256)
#define    USB_HID_UPD_REPORT_SIZE   sizeof(sUSBC_HID_UPD_Report)

#define UPDCC_GET_INFO               1
#define UPDCC_ERASE_RFM			   	     9
#define UPDCC_READ_SOFTWARE_INFO  	 14
#define UPDCC_SET_TX_STATE				   108 
#define UPDCC_GET_RXTX_STATE			   109 
#define UPDCC_GET_UID 					     111 
#define UPDCC_GET_SET_RX_INFO			   112
#define UPDCC_GET_TX_INFO				     113 
#define UPDCC_GET_RX_UPDATE_STATE    116

typedef struct
{
	unsigned long HardwareVersionL;
	unsigned long HardwareVersionH;
	unsigned long BootloaderVersion;
	unsigned long SDRAMSize;
	unsigned long FlashSize;
	unsigned long FlashSectorNb;
	unsigned long SPIFlashSize;
	unsigned long SPIFlashPageSize;
	unsigned long SPIFlashSectorSize;
	unsigned long SPIFlashBlockSize;
	unsigned long FirmwareVersionL;
	unsigned long FirmwareVersionH;
} sUPD_Info;

typedef struct
{
	unsigned long SoftwareVersionL;
	unsigned long SoftwareVersionH;
} sUPD_SoftwareInfo;

typedef struct __attribute__((packed, aligned(1)))
{
	unsigned long CommandNb;
	unsigned long CommandCode;
	unsigned long Parameter1;
	unsigned long Parameter2;
	unsigned long Parameter3;
	unsigned long Parameter4;
	unsigned char Data[REPORT_DATA_SIZE];
} sUSBC_HID_UPD_Report;

typedef struct
{
	unsigned char ReportID;
	sUSBC_HID_UPD_Report Report;
} sReport;

typedef struct
{
	unsigned long NbFiles;
	unsigned long FileSize;
	unsigned long LoadAddress;
	unsigned long StartPage;
	unsigned char Name[16];
	unsigned long CRC32;
	unsigned long VersionL;
	unsigned long VersionH;
} sFileConfig;

typedef struct
{
	unsigned long ID;
	unsigned char ChannelSequence[32];
	unsigned short CompanyCode;
} sTXConfig;


#endif // !defined(USB_UPDATE_H)




