#include "SerialSUMD.h"
#include "CRSF.h"
#include "device.h"

#define SUMD_HEADER_SIZE		3														// 3 Bytes header
#define SUMD_DATA_SIZE_16CH		(16*2)													// 2 Bytes per channel
#define SUMD_CRC_SIZE			2														// 16 bit CRC
#define SUMD_FRAME_16CH_LEN		(SUMD_HEADER_SIZE+SUMD_DATA_SIZE_16CH+SUMD_CRC_SIZE)	

void SerialSUMD::sendByteToFC(uint8_t data)
{
    _outputPort->write((uint8_t)data);
}

void SerialSUMD::setLinkQualityStats(uint16_t lq, uint16_t rssi)
{
    linkQuality = lq;
    rssiDBM = rssi;
}

uint32_t SerialSUMD::sendRCFrameToFC(bool frameAvailable, uint32_t *channelData)
{
    if (!frameAvailable) {
        return DURATION_IMMEDIATELY;
    }

	uint8_t outBuffer[SUMD_FRAME_16CH_LEN];

	outBuffer[0] = 0xA8;		//Graupner
	outBuffer[1] = 0x01;	    //SUMD
	outBuffer[2] = 0x10;		//16CH	

    uint16_t us = (CRSF_to_US(ChannelData[0]) << 3);
    outBuffer[3] = us >> 8;		
    outBuffer[4] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[1]) << 3);
    outBuffer[5] = us >> 8;		
    outBuffer[6] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[2]) << 3);
    outBuffer[7] = us >> 8;		
    outBuffer[8] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[3]) << 3);
    outBuffer[9] = us >> 8;		
    outBuffer[10] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[7]) << 3); //channel 8 mapped to 5 to move arm channel away from the aileron function
    outBuffer[11] = us >> 8;		
    outBuffer[12] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[5]) << 3);
    outBuffer[13] = us >> 8;		
    outBuffer[14] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[6]) << 3);
    outBuffer[15] = us >> 8;		
    outBuffer[16] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[4]) << 3); //channel 5 mapped to 8
    outBuffer[17] = us >> 8;		
    outBuffer[18] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[8]) << 3);
    outBuffer[19] = us >> 8;		
    outBuffer[20] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[9]) << 3);
    outBuffer[21] = us >> 8;		
    outBuffer[22] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[10]) << 3);
    outBuffer[23] = us >> 8;		
    outBuffer[24] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[11]) << 3);
    outBuffer[25] = us >> 8;		
    outBuffer[26] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[12]) << 3);
    outBuffer[27] = us >> 8;		
    outBuffer[28] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[13]) << 3);
    outBuffer[29] = us >> 8;		
    outBuffer[30] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[14]) << 3);
    outBuffer[31] = us >> 8;		
    outBuffer[32] = us & 0x00ff;
    us = (CRSF_to_US(ChannelData[15]) << 3);
    outBuffer[33] = us >> 8;		
    outBuffer[34] = us & 0x00ff;
	  
	uint16_t crc = crc2Byte.calc(outBuffer, (SUMD_HEADER_SIZE + SUMD_DATA_SIZE_16CH), 0);
	outBuffer[35] = (uint8_t)(crc >> 8);
	outBuffer[36] = (uint8_t)(crc & 0x00ff);	  	
	
	_outputPort->write(outBuffer, sizeof(outBuffer));
	
    return DURATION_IMMEDIATELY;
}

void SerialSUMD::sendLinkStatisticsToFC()
{
    // unsupported	
}

void SerialSUMD::sendMSPFrameToFC(uint8_t* data)
{
    (void)data;
    // unsupported
}

void processByte(uint8_t byte) {
    // unsupported
}