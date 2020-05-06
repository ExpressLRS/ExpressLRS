#ifndef __RC_CHANNELS_H
#define __RC_CHANNELS_H

#include "platform.h"
#include "CRSF.h" // N_SWITCHES
#include "msp.h"
#include <stdint.h>

// current and sent switch values
#define N_CONTROLS 4
#define N_SWITCHES 8
#define N_CHANNELS 16 // (N_CONTROLS + N_SWITCHES)

// expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 10 -> sync packet with hop data
// 11 -> tlm packet
enum
{
    RC_DATA_PACKET = 0b00,
    SWITCH_DATA_PACKET = 0b01,
    SYNC_PACKET = 0b10,
    TLM_PACKET = 0b11,
};

enum
{
    DL_PACKET_FREE1 = 0b00,
    DL_PACKET_TLM_MSP = 0b01,
    DL_PACKET_FREE2 = 0b10,
    DL_PACKET_TLM_LINK = 0b11,
};

class RcChannels
{
public:
    RcChannels() {}

    // TX related
    void processChannels(crsf_channels_t const *const channels);
    void ICACHE_RAM_ATTR get_packed_data(uint8_t *const output)
    {
        for (uint8_t i = 0; i < sizeof(packed_buffer); i++)
            output[i] = packed_buffer[i];
    }

    // RX related
    void ICACHE_RAM_ATTR channels_extract(volatile uint8_t const *const input,
                                          crsf_channels_t &output);

    // TLM pkt
    uint8_t ICACHE_RAM_ATTR tlm_send(uint8_t *const output,
                                     mspPacket_t &packet);
    uint8_t ICACHE_RAM_ATTR tlm_receive(volatile uint8_t const *const input,
                                        mspPacket_t &packet);

private:
    void channels_pack(void);
    // Switches / AUX channel handling
    uint8_t getNextSwitchIndex(void);

    // Channel processing data
    volatile uint16_t ChannelDataIn[N_CHANNELS] = {0};  // range: 0...2048
    volatile uint8_t currentSwitches[N_SWITCHES] = {0}; // range: 0,1,2

    // esp requires aligned buffer
    volatile uint8_t WORD_ALIGNED_ATTR packed_buffer[8];

    volatile uint16_t p_auxChannelsChanged = 0; // bitmap of changed switches
    // which switch should be sent in the next rc packet
    volatile uint8_t p_nextSwitchIndex = 0;

#if !defined(HYBRID_SWITCHES_8) && !defined(SEQ_SWITCHES)
    uint32_t SwitchPacketNextSend = 0; //time in ms when the next switch data packet will be send
#define SWITCH_PACKET_SEND_INTERVAL 200u
#endif
};

#endif /* __RC_CHANNELS_H */
