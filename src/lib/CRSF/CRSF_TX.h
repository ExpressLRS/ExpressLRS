#ifndef CRSF_TX_H_
#define CRSF_TX_H_

#include "CRSF.h"

class CRSF_TX : public CRSF
{
public:
    CRSF_TX(HwSerial *dev) : CRSF(dev) {}
    CRSF_TX(HwSerial &dev) : CRSF(dev) {}

    void Begin(void);

    // Handle incoming data
    uint8_t handleUartIn(volatile uint8_t &rx_data_rcvd);

    // Send to RADIO
    void LinkStatisticsSend(void);
    void BatterySensorSend(void);
    void sendLUAresponseToRadio(uint8_t *data, uint8_t len);
    void sendMspPacketToRadio(mspPacket_t &msp);

    // OpenTX Syncing
    void ICACHE_RAM_ATTR setRcPacketRate(uint32_t interval)
    {
#if (FEATURE_OPENTX_SYNC)
        RequestedRCpacketInterval = interval;
#endif
    }

    void ICACHE_RAM_ATTR UpdateOpenTxSyncOffset(uint32_t current_us) // called from timer
    {
#if (FEATURE_OPENTX_SYNC)
        OpenTXsyncOffset = current_us - RCdataLastRecv;
#endif
    }

    ///// Callbacks /////
    static void (*ParamWriteCallback)(uint8_t const *msg, uint16_t len);

    ///// Variables /////

private:
    bool p_slowBaudrate = false;
    volatile bool p_RadioConnected = false; // connected staet

    void uart_wdt(void);
    void processPacket(uint8_t const *input);
    void ICACHE_RAM_ATTR CrsfFramePushToFifo(uint8_t *buff, uint8_t size);
    void LinkStatisticsProcess(void);
    void BatteryStatisticsProcess(void);
    void LuaResponseProcess(void);

#if (FEATURE_OPENTX_SYNC)

#define OpenTXsyncPakcetInterval 200 // in ms
#define RequestedRCpacketAdvance 500 // 800 timing adcance in us

    volatile uint32_t RCdataLastRecv = 0;
    volatile int32_t OpenTXsyncOffset = 0;
    uint32_t RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
    uint32_t OpenTXsynNextSend = 0;
    uint8_t sendSyncPacketToRadio(); // called from main loop

#endif /* FEATURE_OPENTX_SYNC */

    // for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000
    uint32_t p_UartNextCheck = 0;

    uint8_t lua_buff[5];
};

#endif /* CRSF_TX_H_ */
