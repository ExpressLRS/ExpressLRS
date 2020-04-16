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
    void ICACHE_RAM_ATTR sendLinkStatisticsToRadio();
    void ICACHE_RAM_ATTR sendLinkBattSensorToRadio();
    void ICACHE_RAM_ATTR sendLUAresponseToRadio(uint8_t val1, uint8_t val2, uint8_t val3, uint8_t val4);
    void ICACHE_RAM_ATTR sendSetVTXchannelToRadio(uint8_t band, uint8_t channel);

    // OpenTX Syncing
    void ICACHE_RAM_ATTR setRcPacketRate(uint32_t rate)
    {
#if (FEATURE_OPENTX_SYNC)
        RequestedRCpacketInterval = rate;
#endif
    }
    void ICACHE_RAM_ATTR UpdateOpenTxSyncOffset() // called from timer
    {
#if (FEATURE_OPENTX_SYNC)
        OpenTXsyncOffset = micros() - RCdataLastRecv;
#endif
    }

    // Switches / AUX channel handling
    uint16_t ICACHE_RAM_ATTR auxChannelsChanged(uint16_t mask = 0xffff)
    {
        return p_auxChannelsChanged & mask;
    }
    uint8_t ICACHE_RAM_ATTR getNextSwitchIndex(); // TODO: move away

    ///// Variables /////
    volatile uint16_t ChannelDataIn[N_CHANNELS] = {0};  // range: 0...2048
    volatile uint8_t currentSwitches[N_SWITCHES] = {0}; // range: 0,1,2
    volatile uint8_t ParameterUpdateData[2] = {0};      // TODO move this away!

private:
    bool p_slowBaudrate = false;
    volatile uint16_t p_auxChannelsChanged = 0; // bitmap of changed switches

    void uart_wdt(void);
    void processPacket(uint8_t const *input);
    void StoreChannelData(uint8_t const *const data);

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

    // which switch should be sent in the next rc packet
    volatile uint8_t nextSwitchIndex = 0;            // TODO: move away
    volatile uint8_t sentSwitches[N_SWITCHES] = {0}; // TODO: move away
};

#endif /* CRSF_TX_H_ */
