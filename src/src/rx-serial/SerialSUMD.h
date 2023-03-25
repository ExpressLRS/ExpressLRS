#include "SerialIO.h"

#define SUMD_HEADER_SIZE		3														// 3 Bytes header
#define SUMD_DATA_SIZE_16CH		(16*2)													// 2 Bytes per channel
#define SUMD_CRC_SIZE			2														// 16 bit CRC
#define SUMD_FRAME_16CH_LEN		(SUMD_HEADER_SIZE+SUMD_DATA_SIZE_16CH+SUMD_CRC_SIZE)	

#define CRC_POLYNOME 0x1021

class SerialSUMD : public SerialIO {
public:
    explicit SerialSUMD(Stream &out, Stream &in) : SerialIO(&out, &in) {}

    virtual ~SerialSUMD() {}

    void setLinkQualityStats(uint16_t lq, uint16_t rssi) override;
    uint32_t sendRCFrameToFC(bool frameAvailable, uint32_t *channelData) override;
    void sendMSPFrameToFC(uint8_t* data) override;
    void sendLinkStatisticsToFC() override;

private:
    uint16_t linkQuality = 0;
    uint16_t rssiDBM = 0;

    uint16_t crc16(uint8_t *data, uint8_t len);
    void processByte(uint8_t byte) override {};
};
