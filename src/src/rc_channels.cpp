#include "rc_channels.h"
#include "common.h"
#include "helpers.h"
#include "FHSS.h"
#include "debug.h"

#if (N_SWITCHES > (N_CHANNELS - N_CONTROLS))
#error "CRSF Channels Config is not OK"
#endif

#if defined(SEQ_SWITCHES)

typedef struct
{
    // The analog channels
    unsigned rc1 : 11;
    unsigned rc2 : 11;
    unsigned rc3 : 11;
    unsigned rc4 : 10;
    // The round-robin switch
    unsigned aux_n_idx : 3;
    unsigned aux_n : 2;
} PACKED RcDataPacket_s;

/**
 * Sequential switches packet
 * Replaces Generate4ChannelData_11bit
 * Channel 3 is reduced to 10 bits to allow a 3 bit switch index and 2 bit value
 * We cycle through 8 switches on successive packets. If any switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 */
void RcChannels::channels_pack()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr) + UL_PACKET_RC_DATA;
    packed_buffer[0] = PacketHeaderAddr;

    // find the next switch to send
    uint8_t ch_idx = getNextSwitchIndex() & 0b111;

    RcDataPacket_s *rcdata = (RcDataPacket_s *)&packed_buffer[1];
    // The analog channels, ch4 scaled to 10bits
    rcdata->rc1 = (ChannelDataIn[0]);
    rcdata->rc2 = (ChannelDataIn[1]);
    rcdata->rc3 = (ChannelDataIn[2]);
    rcdata->rc4 = (ChannelDataIn[3] >> 1);
    // The round-robin switch
    rcdata->aux_n_idx = ch_idx;
    rcdata->aux_n = currentSwitches[ch_idx] & 0b11;
}

/**
 * Seq switches uses 10 bits for ch3, 3 bits for the switch index and 2 bits for the switch value
 */
void ICACHE_RAM_ATTR RcChannels::channels_extract(volatile uint8_t const *const input,
                                                  crsf_channels_t &PackedRCdataOut)
{
    uint16_t switchValue;
    uint8_t switchIndex;

    RcDataPacket_s *rcdata = (RcDataPacket_s *)&input[1];
    // The analog channels
    PackedRCdataOut.ch0 = rcdata->rc1;
    PackedRCdataOut.ch0 = rcdata->rc2;
    PackedRCdataOut.ch0 = rcdata->rc3;
    PackedRCdataOut.ch0 = ((uint16_t)rcdata->rc4 << 1);
    // The round-robin switch
    switchIndex = rcdata->aux_n_idx;
    switchValue = SWITCH2b_to_CRSF(rcdata->aux_n);

    switch (switchIndex)
    {
        case 0:
            PackedRCdataOut.ch4 = switchValue;
            break;
        case 1:
            PackedRCdataOut.ch5 = switchValue;
            break;
        case 2:
            PackedRCdataOut.ch6 = switchValue;
            break;
        case 3:
            PackedRCdataOut.ch7 = switchValue;
            break;
        case 4:
            PackedRCdataOut.ch8 = switchValue;
            break;
        case 5:
            PackedRCdataOut.ch9 = switchValue;
            break;
        case 6:
            PackedRCdataOut.ch10 = switchValue;
            break;
        case 7:
            PackedRCdataOut.ch11 = switchValue;
            break;
    }
}

#elif defined(HYBRID_SWITCHES_8)

typedef struct
{
    // The analog channels
    unsigned rc1 : 10;
    unsigned rc2 : 10;
    unsigned rc3 : 10;
    unsigned rc4 : 10;
    // The low latency switch
    unsigned aux1 : 2;
    // The round-robin switch
    unsigned aux_n_idx : 3;
    unsigned aux_n : 3; // 2b or 3b switches
} PACKED RcDataPacket_s;

/**
 * Hybrid switches packet
 * Replaces Generate4ChannelData_11bit
 * Analog channels are reduced to 10 bits to allow for switch encoding
 * Switch[0] is sent on every packet.
 * A 3 bit switch index and 2 bit value is used to send the remaining switches
 * in a round-robin fashion.
 * If any of the round-robin switches have changed
 * we take the lowest indexed one and send that, hence lower indexed switches have
 * higher priority in the event that several are changed at once.
 */
void RcChannels::channels_pack()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr) + UL_PACKET_RC_DATA;
    packed_buffer[0] = PacketHeaderAddr;

    // find the next switch to send
    uint8_t ch_idx = getNextSwitchIndex() & 0b111; // mask for paranoia

    RcDataPacket_s *rcdata = (RcDataPacket_s *)&packed_buffer[1];
    // The analog channels, scale down to 10bits
    rcdata->rc1 = (ChannelDataIn[0] >> 1);
    rcdata->rc2 = (ChannelDataIn[1] >> 1);
    rcdata->rc3 = (ChannelDataIn[2] >> 1);
    rcdata->rc4 = (ChannelDataIn[3] >> 1);
    // The low latency switch
    rcdata->aux1 = currentSwitches[0] & 0b111;
    // The round-robin switch
    rcdata->aux_n_idx = ch_idx;
    rcdata->aux_n = currentSwitches[ch_idx] & 0b111;
}

/**
 * Hybrid switches uses 10 bits for each analog channel,
 * 2 bits for the low latency switch[0]
 * 3 bits for the round-robin switch index and 2 bits for the value
 */
void ICACHE_RAM_ATTR RcChannels::channels_extract(volatile uint8_t const *const input,
                                                  crsf_channels_t &PackedRCdataOut)
{
    RcDataPacket_s *rcdata = (RcDataPacket_s *)&input[1];
    // The analog channels, scaled back to 11bits
    PackedRCdataOut.ch0 = ((uint16_t)rcdata->rc1 << 1);
    PackedRCdataOut.ch1 = ((uint16_t)rcdata->rc2 << 1);
    PackedRCdataOut.ch2 = ((uint16_t)rcdata->rc3 << 1);
    PackedRCdataOut.ch3 = ((uint16_t)rcdata->rc4 << 1);
    // The low latency switch
    PackedRCdataOut.ch4 = SWITCH2b_to_CRSF(rcdata->aux1);
    // The round-robin switch
    uint8_t switchIndex = rcdata->aux_n_idx;
    uint16_t switchValue = SWITCH2b_to_CRSF(rcdata->aux_n);

    switch (switchIndex)
    {
        case 0: // we should never get index 0 here since that is the low latency switch
            DEBUG_PRINTLN("BAD switchIndex 0");
            break;
        case 1:
            PackedRCdataOut.ch5 = switchValue;
            break;
        case 2:
            PackedRCdataOut.ch6 = switchValue;
            break;
        case 3:
            PackedRCdataOut.ch7 = switchValue;
            break;
        case 4:
            PackedRCdataOut.ch8 = switchValue;
            break;
        case 5:
            PackedRCdataOut.ch9 = switchValue;
            break;
        case 6:
            PackedRCdataOut.ch10 = switchValue;
            break;
        case 7:
            PackedRCdataOut.ch11 = switchValue;
            break;
    }
}

#else

typedef struct
{
    // Swtiches
    unsigned aux1 : 4;
    unsigned aux2 : 4;
    unsigned aux3 : 4;
    unsigned aux4 : 4;
} PACKED SwitchPacket_s;

typedef struct
{
    // The analog channels
    unsigned rc1 : 11;
    unsigned rc2 : 11;
    unsigned rc3 : 11;
    unsigned rc4 : 11;
    // One_Bit_Switches
    unsigned aux1 : 1;
    unsigned aux2 : 1;
    unsigned aux3 : 1;
    unsigned aux4 : 1;
} PACKED RcDataPacket_s;

extern volatile uint32_t DRAM_ATTR _rf_rxtx_counter;

void RcChannels::channels_pack()
{
    uint32_t current_ms = millis();
    uint8_t PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr);

#ifndef One_Bit_Switches
    if ((current_ms > SwitchPacketNextSend) || (p_auxChannelsChanged & 0xf))
    {
        SwitchPacketNextSend = current_ms + SWITCH_PACKET_SEND_INTERVAL;
        p_auxChannelsChanged = 0;

        packed_buffer[0] = PacketHeaderAddr + UL_PACKET_SWITCH_DATA;
        SwitchPacket_s *rcdata = (SwitchPacket_s *)&packed_buffer[1];
        rcdata->aux1 = (CRSF_to_UINT10(ChannelDataIn[4]) >> 6);
        rcdata->aux2 = (CRSF_to_UINT10(ChannelDataIn[5]) >> 6);
        rcdata->aux3 = (CRSF_to_UINT10(ChannelDataIn[6]) >> 6);
        rcdata->aux4 = (CRSF_to_UINT10(ChannelDataIn[7]) >> 6);
        packed_buffer[3] = packed_buffer[1];
        packed_buffer[4] = packed_buffer[2];
        packed_buffer[5] = _rf_rxtx_counter;
        packed_buffer[6] = FHSSgetCurrIndex();
    }
    else // else we just have regular channel data which we send as 8 + 2 bits
#endif // !One_Bit_Switches
    {
        packed_buffer[0] = PacketHeaderAddr + UL_PACKET_RC_DATA;

        RcDataPacket_s *rcdata = (RcDataPacket_s *)&packed_buffer[1];
        rcdata->rc1 = ChannelDataIn[0];
        rcdata->rc2 = ChannelDataIn[1];
        rcdata->rc3 = ChannelDataIn[2];
        rcdata->rc4 = ChannelDataIn[3];
#ifdef One_Bit_Switches
        rcdata->aux1 = CRSF_to_BIT(ChannelDataIn[4]);
        rcdata->aux2 = CRSF_to_BIT(ChannelDataIn[5]);
        rcdata->aux3 = CRSF_to_BIT(ChannelDataIn[6]);
        rcdata->aux4 = CRSF_to_BIT(ChannelDataIn[7]);
#endif // One_Bit_Switches
    }
}

void ICACHE_RAM_ATTR RcChannels::channels_extract(volatile uint8_t const *const input,
                                                  crsf_channels_t &PackedRCdataOut)
{
    uint8_t type = input[0] & 0b11;
    if (type == UL_PACKET_RC_DATA)
    {
        RcDataPacket_s *rcdata = (RcDataPacket_s *)&input[1];
        PackedRCdataOut.ch0 = rcdata->rc1;
        PackedRCdataOut.ch1 = rcdata->rc2;
        PackedRCdataOut.ch2 = rcdata->rc3;
        PackedRCdataOut.ch3 = rcdata->rc4;
#ifdef One_Bit_Switches
        PackedRCdataOut.ch4 = BIT_to_CRSF(rcdata->aux1);
        PackedRCdataOut.ch5 = BIT_to_CRSF(rcdata->aux2);
        PackedRCdataOut.ch6 = BIT_to_CRSF(rcdata->aux3);
        PackedRCdataOut.ch7 = BIT_to_CRSF(rcdata->aux4);
#endif // One_Bit_Switches
    }
    else if (type == UL_PACKET_SWITCH_DATA)
    {
        SwitchPacket_s *rcdata = (SwitchPacket_s *)&input[1];
        PackedRCdataOut.ch4 = SWITCH3b_to_CRSF(rcdata->aux1);
        PackedRCdataOut.ch5 = SWITCH3b_to_CRSF(rcdata->aux2);
        PackedRCdataOut.ch6 = SWITCH3b_to_CRSF(rcdata->aux3);
        PackedRCdataOut.ch7 = SWITCH3b_to_CRSF(rcdata->aux4);
    }
}
#endif

void RcChannels::processChannels(crsf_channels_t const *const rcChannels)
{
    uint8_t switch_state;

    // TODO: loop N_CHANNELS times
    ChannelDataIn[0] = (rcChannels->ch0);
    ChannelDataIn[1] = (rcChannels->ch1);
    ChannelDataIn[2] = (rcChannels->ch2);
    ChannelDataIn[3] = (rcChannels->ch3);
    ChannelDataIn[4] = (rcChannels->ch4);
    ChannelDataIn[5] = (rcChannels->ch5);
    ChannelDataIn[6] = (rcChannels->ch6);
    ChannelDataIn[7] = (rcChannels->ch7);
    ChannelDataIn[8] = (rcChannels->ch8);
    ChannelDataIn[9] = (rcChannels->ch9);
    ChannelDataIn[10] = (rcChannels->ch10);
    ChannelDataIn[11] = (rcChannels->ch11);
    ChannelDataIn[12] = (rcChannels->ch12);
    ChannelDataIn[13] = (rcChannels->ch13);
    ChannelDataIn[14] = (rcChannels->ch14);
    ChannelDataIn[15] = (rcChannels->ch15);

    /**
     * Convert the rc data corresponding to switches to 2 bit values.
     *
     * I'm defining channels 4 through 11 inclusive as representing switches
     * Take the input values and convert them to the range 0 - 2.
     * (not 0-3 because most people use 3 way switches and expect the middle
     *  position to be represented by a middle numeric value)
     */
    for (uint8_t i = 0; i < N_SWITCHES; i++)
    {
        // input is 0 - 2048, output is 0 - 2
        switch_state = CRSF_to_SWITCH2b(ChannelDataIn[i + N_CONTROLS]) & 0b11; //ChannelDataIn[i + N_CONTROLS] / 682;
        // Check if state is changed
        if (switch_state != currentSwitches[i])
            p_auxChannelsChanged |= (0x1 << i);
        currentSwitches[i] = switch_state;
    }

    channels_pack();
}

/**
 * Determine which switch to send next.
 * If any switch has changed since last sent, we send the lowest index changed switch
 * and set nextSwitchIndex to that value + 1.
 * If no switches have changed then we send nextSwitchIndex and increment the value.
 * For pure sequential switches, all 8 switches are part of the round-robin sequence.
 * For hybrid switches, switch 0 is sent with every packet and the rest of the switches
 * are in the round-robin.
 */
uint8_t RcChannels::getNextSwitchIndex()
{
    int8_t i;
#ifdef HYBRID_SWITCHES_8
#define firstSwitch 1 // skip 0 since it is sent on every packet
#else
#define firstSwitch 0 // sequential switches includes switch 0
#endif

#ifdef HYBRID_SWITCHES_8
    // for hydrid switches 0 is sent on every packet
    p_auxChannelsChanged &= 0xfffe;
#endif
    i = __builtin_ffs(p_auxChannelsChanged) - 1;
    if (i < 0)
    {
        i = p_nextSwitchIndex++;
        p_nextSwitchIndex %= N_SWITCHES;
    }
    else
    {
        p_auxChannelsChanged &= ~(0x1 << i);
    }

#ifdef HYBRID_SWITCHES_8
    // for hydrid switches 0 is sent on every packet, so we can skip
    // that value for the round-robin
    if (p_nextSwitchIndex == 0)
        p_nextSwitchIndex = firstSwitch;
#endif

    return i;
}

typedef union {
    struct
    {
        uint8_t address; // just to align
        uint8_t flags;
        uint16_t func;
        uint16_t payloadSize;
        uint8_t type;
    } hdr;
    struct
    {
        uint8_t address; // just to align
        uint8_t data[6];
    } payload;
} TlmDataPacket_s;

uint8_t ICACHE_RAM_ATTR RcChannels::tlm_send(uint8_t *const output,
                                             mspPacket_t &packet)
{
    TlmDataPacket_s *tlm_ptr = (TlmDataPacket_s *)output;

    /* Ignore invalid packets */
    if (packet.type != MSP_PACKET_TLM_OTA)
        return 0;

    tlm_ptr->hdr.address = DEIVCE_ADDR_GENERATE(DeviceAddr) + UL_PACKET_MSP;

    if (!packet.header_sent_or_rcvd)
    {
        /* Send header and first byte */
        tlm_ptr->hdr.type = MSP_PACKET_TLM_OTA;
        tlm_ptr->hdr.flags = packet.flags;
        tlm_ptr->hdr.func = packet.function;
        tlm_ptr->hdr.payloadSize = packet.payloadSize;
        packet.header_sent_or_rcvd = true;
    }
    else
    {
        for (uint8_t iter = 0; iter < sizeof(tlm_ptr->payload.data); iter++)
            tlm_ptr->payload.data[iter] = packet.readByte();
    }
    return packet.iterated();
}

uint8_t ICACHE_RAM_ATTR RcChannels::tlm_receive(volatile uint8_t const *const input,
                                                mspPacket_t &packet)
{
    TlmDataPacket_s *tlm_ptr = (TlmDataPacket_s *)input;
    if (packet.header_sent_or_rcvd && packet.type == MSP_PACKET_TLM_OTA)
    {
        for (uint8_t iter = 0; iter < sizeof(tlm_ptr->payload.data); iter++)
            packet.addByte(tlm_ptr->payload.data[iter]);
    }
    else if (packet.type == MSP_PACKET_UNKNOWN && tlm_ptr->hdr.type == MSP_PACKET_TLM_OTA)
    {
        // buffer free, fill header
        packet.type = MSP_PACKET_TLM_OTA;
        packet.flags = tlm_ptr->hdr.flags;
        packet.function = tlm_ptr->hdr.func;
        packet.payloadSize = tlm_ptr->hdr.payloadSize;
        packet.header_sent_or_rcvd = true;
    }
    else
    {
        return 0;
    }
    return packet.iterated();
}
