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
    void handleUartIn(void);

    // Send to RADIO
    void ICACHE_RAM_ATTR LinkStatisticsSend(void);
    void ICACHE_RAM_ATTR BatterySensorSend(void);
    void ICACHE_RAM_ATTR sendLUAresponseToRadio(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4);
    void ICACHE_RAM_ATTR sendSetVTXchannelToRadio(uint8_t band, uint8_t channel);

    // OpenTX Syncing
    void ICACHE_RAM_ATTR setRcPacketRate(uint32_t interval)
    {
        switch (interval)
        {
        case 5000: // 200Hz
            LinkStatistics.rf_Mode = 3;
            break;
        case 10000: // 100Hz
            LinkStatistics.rf_Mode = 2;
            break;
        case 20000: // 50Hz
            LinkStatistics.rf_Mode = 1;
            break;
        case 40000:  // 25Hz
        case 250000: // 4Hz
        default:
            LinkStatistics.rf_Mode = 0;
            break;
        };
#if (FEATURE_OPENTX_SYNC)
        RequestedRCpacketInterval = interval;
#endif
    }
    void ICACHE_RAM_ATTR UpdateOpenTxSyncOffset() // called from timer
    {
#if (FEATURE_OPENTX_SYNC)
        OpenTXsyncOffset = micros() - RCdataLastRecv;
#endif
    }

    ///// Variables /////
    volatile uint8_t ParameterUpdateData[2] = {0}; // TODO move this away!

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
