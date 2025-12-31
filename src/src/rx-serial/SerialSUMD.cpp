#include "SerialSUMD.h"

#include "crsf_protocol.h"
#include "device.h"

#define SUMD_HEADER_SIZE		3														// 3 Bytes header
#define SUMD_DATA_SIZE_16CH		(16*2)													// 2 Bytes per channel
#define SUMD_CRC_SIZE			2														// 16 bit CRC
#define SUMD_FRAME_16CH_LEN		(SUMD_HEADER_SIZE+SUMD_DATA_SIZE_16CH+SUMD_CRC_SIZE)

const auto SUMD_CALLBACK_INTERVAL_MS = 10;

#if defined(WMEXTENSION)
#include "common.h"
#include "RXEndpoint.h"
extern RXEndpoint crsfReceiver;
#endif

uint32_t SerialSUMD::sendRCFrame(bool frameAvailable, bool frameMissed, uint32_t *channelData)
{
    if (!frameAvailable) {
        return DURATION_IMMEDIATELY;
    }

	  uint8_t outBuffer[SUMD_FRAME_16CH_LEN];

	  outBuffer[0] = 0xA8;		//Graupner
	  outBuffer[1] = 0x01;	  //SUMD
	  outBuffer[2] = 0x10;		//16CH

    uint16_t us = (CRSF_to_US(channelData[0]) << 3);
    outBuffer[3] = us >> 8;
    outBuffer[4] = us & 0x00ff;
    us = (CRSF_to_US(channelData[1]) << 3);
    outBuffer[5] = us >> 8;
    outBuffer[6] = us & 0x00ff;
    us = (CRSF_to_US(channelData[2]) << 3);
    outBuffer[7] = us >> 8;
    outBuffer[8] = us & 0x00ff;
    us = (CRSF_to_US(channelData[3]) << 3);
    outBuffer[9] = us >> 8;
    outBuffer[10] = us & 0x00ff;
    us = (CRSF_to_US(channelData[7]) << 3); //channel 8 mapped to 5 to move arm channel away from the aileron function
    outBuffer[11] = us >> 8;
    outBuffer[12] = us & 0x00ff;
    us = (CRSF_to_US(channelData[5]) << 3);
    outBuffer[13] = us >> 8;
    outBuffer[14] = us & 0x00ff;
    us = (CRSF_to_US(channelData[6]) << 3);
    outBuffer[15] = us >> 8;
    outBuffer[16] = us & 0x00ff;
    us = (CRSF_to_US(channelData[4]) << 3); //channel 5 mapped to 8
    outBuffer[17] = us >> 8;
    outBuffer[18] = us & 0x00ff;
    us = (CRSF_to_US(channelData[8]) << 3);
    outBuffer[19] = us >> 8;
    outBuffer[20] = us & 0x00ff;
    us = (CRSF_to_US(channelData[9]) << 3);
    outBuffer[21] = us >> 8;
    outBuffer[22] = us & 0x00ff;
    us = (CRSF_to_US(channelData[10]) << 3);
    outBuffer[23] = us >> 8;
    outBuffer[24] = us & 0x00ff;
    us = (CRSF_to_US(channelData[11]) << 3);
    outBuffer[25] = us >> 8;
    outBuffer[26] = us & 0x00ff;
    us = (CRSF_to_US(channelData[12]) << 3);
    outBuffer[27] = us >> 8;
    outBuffer[28] = us & 0x00ff;
    us = (CRSF_to_US(channelData[13]) << 3);
    outBuffer[29] = us >> 8;
    outBuffer[30] = us & 0x00ff;
    us = (CRSF_to_US(channelData[14]) << 3);
    outBuffer[31] = us >> 8;
    outBuffer[32] = us & 0x00ff;
    us = (CRSF_to_US(channelData[15]) << 3);
    outBuffer[33] = us >> 8;
    outBuffer[34] = us & 0x00ff;

	  uint16_t crc = crc2Byte.calc(outBuffer, (SUMD_HEADER_SIZE + SUMD_DATA_SIZE_16CH), 0);
	  outBuffer[35] = (uint8_t)(crc >> 8);
	  outBuffer[36] = (uint8_t)(crc & 0x00ff);

	  _outputPort->write(outBuffer, sizeof(outBuffer));

    return SUMD_CALLBACK_INTERVAL_MS;
}

#if defined(WMEXTENSION)

#define SUMD3_HEADER_SIZE		3														// 3 Bytes header
#define SUMD3_DATA_SIZE	        (16*2)													// 2 Bytes per channel
#define SUMD3_CMD_SIZE	        4													//
#define SUMD3_CRC_SIZE			2														// 16 bit CRC
#define SUMD3_FRAME_LEN	(SUMD3_HEADER_SIZE + SUMD3_DATA_SIZE + SUMD3_CMD_SIZE + SUMD3_CRC_SIZE)

uint32_t SerialSUMD3::sendRCFrame(const bool frameAvailable, const bool frameMissed,
                                  uint32_t* const channelData)
{
    static_assert(SUMD3_FRAME_LEN == 41);

    if (!frameAvailable) {
        return DURATION_IMMEDIATELY;
    }

    uint8_t outBuffer[SUMD3_FRAME_LEN];

    outBuffer[0] = 0xA8;		//Graupner
    outBuffer[1] = 0x03;	    //SUMD
    outBuffer[2] = 18; 		//16CH

    switch(state) {
    case State::CH1_16:
        composeFrame(0x02, channelData, outBuffer);
        state = State::CH1_8_17_24;
        break;
    case State::CH1_8_17_24:
        composeFrame(0x03, channelData, outBuffer);
        state = State::CH1_8_25_32;
        break;
    case State::CH1_8_25_32:
        composeFrame(0x04, channelData, outBuffer);
        state = State::CH1_12_SW;
        break;
    case State::CH1_12_SW:
        composeFrame(0x05, channelData, outBuffer);
        state = State::CH1_16;
        break;
    }

    uint16_t crc = crc2Byte.calc(outBuffer, (SUMD3_HEADER_SIZE + SUMD3_DATA_SIZE + SUMD3_CMD_SIZE), 0);
    outBuffer[39] = (uint8_t)(crc >> 8);
    outBuffer[40] = (uint8_t)(crc & 0x00ff);

    _outputPort->write(outBuffer, sizeof(outBuffer));

    return SUMD_CALLBACK_INTERVAL_MS;
}

void SerialSUMD3::composeFrame(const uint8_t fCode, const uint32_t* const channelData, uint8_t* const data) {
    const uint8_t cflags = crsfReceiver.multiSwitch().channelFlags();
    uint8_t i = 3;
    if (fCode == 0x02) {
        for(uint8_t c = 0; c < 16; c++) {
            if (cflags & 0b10) {
                const uint16_t us = (CRSF_to_US(channelData[c + CRSF_NUM_CHANNELS]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
            else {
                const uint16_t us = (CRSF_to_US(channelData[c]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
    }
    else if (fCode == 0x03) {
        if (cflags & 0b10) {
            for(uint8_t c = 0; c < 8; c++) {
                const uint16_t us = (CRSF_to_US(channelData[c + CRSF_NUM_CHANNELS]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
            for(uint8_t c = 16; c < 24; c++) {
                const uint16_t us = (CRSF_to_US(channelData[c - 16]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
        else {
            for(uint8_t c = 0; c < 8; c++) {
                const uint16_t us = (CRSF_to_US(channelData[c]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
            for(uint8_t c = 16; c < 24; c++) {
                const uint16_t us = (CRSF_to_US(channelData[c]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
    }
    else if (fCode == 0x04) {
        if (cflags & 0b10) {
            for(uint8_t c = 0; c < 8; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c + CRSF_NUM_CHANNELS]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
            for(uint8_t c = 24; c < 32; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c - 24]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
        else {
            for(uint8_t c = 0; c < 8; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
            for(uint8_t c = 24; c < 32; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c + CRSF_NUM_CHANNELS - 24]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
    }
    else if (fCode == 0x05) {
        if (cflags & 0b10) {
            for(uint8_t c = 0; c < 12; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c + CRSF_NUM_CHANNELS]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
        else {
            for(uint8_t c = 0; c < 12; ++c) {
                const uint16_t us = (CRSF_to_US(channelData[c]) << 3);
                data[i++] = us >> 8;
                data[i++] = us & 0x00ff;
            }
        }
        data[i++] = crsfReceiver.multiSwitch().switches()[1];
        data[i++] = crsfReceiver.multiSwitch().switches()[0];
        data[i++] = crsfReceiver.multiSwitch().switches()[3];
        data[i++] = crsfReceiver.multiSwitch().switches()[2];
        data[i++] = crsfReceiver.multiSwitch().switches()[5];
        data[i++] = crsfReceiver.multiSwitch().switches()[4];
        data[i++] = crsfReceiver.multiSwitch().switches()[7];
        data[i++] = crsfReceiver.multiSwitch().switches()[6];
    }
    data[i++] = fCode;
    data[i++] = 0; // res
    data[i++] = 0; // mode
    data[i++] = 0; // sub
}

#endif
