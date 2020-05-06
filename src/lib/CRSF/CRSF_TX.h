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
    void handleUartIn(volatile uint8_t &rx_data_rcvd);

    // Send to RADIO
    void LinkStatisticsSend(void);
    void BatterySensorSend(void);
    void sendLUAresponseToRadio(uint8_t *data, uint8_t len);
    void sendMspPacketToRadio(mspPacket_t &msp);
    void sendSetVTXchannelToRadio(uint8_t band, uint8_t channel);

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

    void uart_wdt(void);
    void processPacket(uint8_t const *input);

#if (FEATURE_OPENTX_SYNC)

#define OpenTXsyncPakcetInterval 100 // in ms
#define RequestedRCpacketAdvance 500 // 800 timing adcance in us

    volatile uint32_t RCdataLastRecv = 0;
    volatile int32_t OpenTXsyncOffset = 0;
    uint32_t RequestedRCpacketInterval = 5000; // default to 200hz as per 'normal'
    uint32_t OpenTXsynNextSend = 0;
    void sendSyncPacketToRadio(); // called from main loop

#endif /* FEATURE_OPENTX_SYNC */

    // for the UART wdt, every 1000ms we change bauds when connect is lost
#define UARTwdtInterval 1000
    uint32_t p_UartNextCheck = 0;
};

#endif /* CRSF_TX_H_ */
