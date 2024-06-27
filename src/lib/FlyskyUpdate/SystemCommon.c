#include "SystemCommon.h"	

volatile unsigned long* 	ResetSignature = (unsigned long*) 0x20000000;

volatile unsigned long*  	RFM_TXID =                     (unsigned long*) CONFIG_BASE_ADDRESS;

volatile unsigned long*  ProductNumber       =      	(unsigned long*) 0x08000200;
volatile unsigned long*  HardwareVersionH     =     	(unsigned long*) 0x08000204;
volatile unsigned long*  HardwareVersionL      =    	(unsigned long*) 0x08000208;
volatile unsigned long*  BootloaderVersion       =    (unsigned long*) 0x08000210;

const __attribute__((section (".firmware_signature"))) unsigned long   FirmwareSignature[2] = {FIRMWARE_SIGNATURE_1, FIRMWARE_SIGNATURE_2};
const __attribute__((section (".firmware_length"))) unsigned long  	FirmwareLength = 0x12345678;
const __attribute__((section (".firmware_versionH"))) unsigned long  	FirmwareVersionH = 0x00010000;
const __attribute__((section (".firmware_versionL"))) unsigned long  	FirmwareVersionL = 0x00010001;
const __attribute__((section (".rf_library_version"))) unsigned long  	RFLibraryVersion = 0x00030000;
const __attribute__((section (".firmware_reserved"))) unsigned long  	FirmwareReserved[0x68/4];




