#if !defined(SYSTEM_COMMON_H)
#define SYSTEM_COMMON_H

	#define FLASH_BASE_ADDRESS    	0x08000000
	#define FLASH_SIZE            	0x20000	
	#define BOOTLOADER_SIZE       	0x6000  
	#define MAX_FIRMWARE_SIZE     	(FLASH_SIZE-BOOTLOADER_SIZE-4)	

	#define CONFIG_BASE_ADDRESS   	(FLASH_BASE_ADDRESS+BOOTLOADER_SIZE-0x800)
	#define FIRMWARE_BASE_ADDRESS 	(FLASH_BASE_ADDRESS+BOOTLOADER_SIZE)		
	
	#define RESET_SIGNATURE_1 			0x1AE811C9
	#define RESET_SIGNATURE_2 			0x7F5594AC
		
	#define FIRMWARE_SIGNATURE_1 		0xC87251CC
	#define FIRMWARE_SIGNATURE_2 		0x92017EA0
	
	extern volatile unsigned long*  ResetSignature;
	extern volatile unsigned long* 	RFM_TXID;
	
  extern volatile unsigned long*  ProductNumber;
	extern volatile unsigned long*  HardwareVersionH;
	extern volatile unsigned long*  HardwareVersionL;
	extern volatile unsigned long*  BootloaderVersion;
		
	extern const unsigned long 		  FirmwareSignature[2];
	extern const unsigned long 	  	FirmwareLength;
	extern const unsigned long 		  FirmwareVersionH;
	extern const unsigned long 	  	FirmwareVersionL;
	extern const unsigned long 	  	RFLibraryVersion;

#endif // !defined(SYSTEM_COMMON_H)

