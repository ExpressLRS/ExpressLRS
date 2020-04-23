#include "rc_channels.h"
#include "common.h"
#include "helpers.h"

#include "LoRaRadioLib.h"
#include "FHSS.h"

#define USE_RC_DATA_STRUCTS 0

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
//void GenerateChannelDataSeqSwitch()
void RcChannels::channels_pack()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr) + RC_DATA_PACKET;
    packed_buffer[0] = PacketHeaderAddr;

    // find the next switch to send
    uint8_t ch_idx = getNextSwitchIndex() & 0b111;

#if USE_RC_DATA_STRUCTS
    RcDataPacket_s *rcdata = (RcDataPacket_s *)&packed_buffer[1];
    // The analog channels
    rcdata->rc1 = (ChannelDataIn[0]);
    rcdata->rc2 = (ChannelDataIn[1]);
    rcdata->rc3 = (ChannelDataIn[2]);
    rcdata->rc4 = (ChannelDataIn[3] >> 1);
    // The round-robin switch
    rcdata->aux_n_idx = ch_idx;
    rcdata->aux_n = currentSwitches[ch_idx] & 0b11;
#else
    packed_buffer[1] = ((ChannelDataIn[0]) >> 3);
    packed_buffer[2] = ((ChannelDataIn[1]) >> 3);
    packed_buffer[3] = ((ChannelDataIn[2]) >> 3);
    packed_buffer[4] = ((ChannelDataIn[3]) >> 3);
    packed_buffer[5] = ((ChannelDataIn[0] & 0b00000111) << 5) +
                       ((ChannelDataIn[1] & 0b111) << 2) +
                       ((ChannelDataIn[2] & 0b110) >> 1);
    packed_buffer[6] = ((ChannelDataIn[2] & 0b001) << 7) +
                       ((ChannelDataIn[3] & 0b110) << 4);

    // put the bits into buf[6]
    packed_buffer[6] += (ch_idx << 2) + (currentSwitches[ch_idx] & 0b11);
#endif
}

/**
 * Seq switches uses 10 bits for ch3, 3 bits for the switch index and 2 bits for the switch value
 */
//void UnpackChannelDataSeqSwitches()
void RcChannels::channels_extract(volatile uint8_t const *const input,
                                  crsf_channels_t &PackedRCdataOut)
{
    uint16_t switchValue;
    uint8_t switchIndex;

#if USE_RC_DATA_STRUCTS
    RcDataPacket_s *rcdata = (RcDataPacket_s *)&input[1];
    // The analog channels
    PackedRCdataOut.ch0 = rcdata->rc1;
    PackedRCdataOut.ch0 = rcdata->rc2;
    PackedRCdataOut.ch0 = rcdata->rc3;
    PackedRCdataOut.ch0 = rcdata->rc4;
    // The round-robin switch
    switchIndex = rcdata->aux_n_idx;
    switchValue = SWITCH2b_to_CRSF(rcdata->aux_n);
#else
    PackedRCdataOut.ch0 = (input[1] << 3) + ((input[5] & 0b11100000) >> 5);
    PackedRCdataOut.ch1 = (input[2] << 3) + ((input[5] & 0b00011100) >> 2);
    PackedRCdataOut.ch2 = (input[3] << 3) + ((input[5] & 0b00000011) << 1) + (input[6] & 0b10000000 >> 7);
    PackedRCdataOut.ch3 = (input[4] << 3) + ((input[6] & 0b01100000) >> 4);

    switchIndex = (input[6] & 0b11100) >> 2;
    switchValue = SWITCH2b_to_CRSF(input[6] & 0b11);
#endif

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
    unsigned aux_n : 2;
    unsigned _fill : 1; // one bit is free
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
//void GenerateChannelDataHybridSwitch8()
void RcChannels::channels_pack()
{
    uint8_t PacketHeaderAddr;
    PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr) + RC_DATA_PACKET;
    packed_buffer[0] = PacketHeaderAddr;

    // find the next switch to send
    uint8_t ch_idx = getNextSwitchIndex() & 0b111; // mask for paranoia

#if USE_RC_DATA_STRUCTS
    RcDataPacket_s *rcdata = (RcDataPacket_s *)&packed_buffer[1];
    // The analog channels
    rcdata->rc1 = (ChannelDataIn[0] >> 1);
    rcdata->rc2 = (ChannelDataIn[1] >> 1);
    rcdata->rc3 = (ChannelDataIn[2] >> 1);
    rcdata->rc4 = (ChannelDataIn[3] >> 1);
    // The low latency switch
    rcdata->aux1 = currentSwitches[0] & 0b11;
    // The round-robin switch
    rcdata->aux_n_idx = ch_idx;
    rcdata->aux_n = currentSwitches[ch_idx] & 0b11;
#else
    packed_buffer[1] = ((ChannelDataIn[0]) >> 3);
    packed_buffer[2] = ((ChannelDataIn[1]) >> 3);
    packed_buffer[3] = ((ChannelDataIn[2]) >> 3);
    packed_buffer[4] = ((ChannelDataIn[3]) >> 3);
    packed_buffer[5] = ((ChannelDataIn[0] & 0b110) << 5) +
                       ((ChannelDataIn[1] & 0b110) << 3) +
                       ((ChannelDataIn[2] & 0b110) << 1) +
                       ((ChannelDataIn[3] & 0b110) >> 1);

    // switch 0 is sent on every packet - intended for low latency arm/disarm
    packed_buffer[6] = (currentSwitches[0] & 0b11) << 5; // note this leaves the top bit of byte 6 unused

    // put the bits into buf[6]. i is in the range 1 through 7 so takes 3 bits
    // currentSwitches[i] is in the range 0 through 2, takes 2 bits.
    packed_buffer[6] += (ch_idx << 2) + (currentSwitches[ch_idx] & 0b11); // mask for paranoia
#endif
}

/**
 * Hybrid switches uses 10 bits for each analog channel,
 * 2 bits for the low latency switch[0]
 * 3 bits for the round-robin switch index and 2 bits for the value
 */
//void UnpackChannelDataHybridSwitches8()
void RcChannels::channels_extract(volatile uint8_t const *const input,
                                  crsf_channels_t &PackedRCdataOut)
{
#if USE_RC_DATA_STRUCTS
    RcDataPacket_s *rcdata = (RcDataPacket_s *)&input[1];
    // The analog channels
    PackedRCdataOut.ch0 = rcdata->rc1;
    PackedRCdataOut.ch1 = rcdata->rc2;
    PackedRCdataOut.ch2 = rcdata->rc3;
    PackedRCdataOut.ch3 = rcdata->rc4;
    // The low latency switch
    PackedRCdataOut.ch4 = SWITCH2b_to_CRSF(rcdata->aux1);
    // The round-robin switch
    uint8_t switchIndex = rcdata->aux_n_idx;
    uint16_t switchValue = SWITCH2b_to_CRSF(rcdata->aux_n);
#else
    // The analog channels
    PackedRCdataOut.ch0 = (input[1] << 3) + ((input[5] & 0b11000000) >> 5);
    PackedRCdataOut.ch1 = (input[2] << 3) + ((input[5] & 0b00110000) >> 3);
    PackedRCdataOut.ch2 = (input[3] << 3) + ((input[5] & 0b00001100) >> 1);
    PackedRCdataOut.ch3 = (input[4] << 3) + ((input[5] & 0b00000011) << 1);

    // The low latency switch
    PackedRCdataOut.ch4 = SWITCH2b_to_CRSF((input[6] & 0b01100000) >> 5);

    // The round-robin switch
    uint8_t switchIndex = (input[6] & 0b11100) >> 2;
    uint16_t switchValue = SWITCH2b_to_CRSF(input[6] & 0b11);
#endif

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

//void Generate4ChannelData_11bit()
void RcChannels::channels_pack()
{
    uint32_t current_ms = millis();
    uint8_t PacketHeaderAddr = DEIVCE_ADDR_GENERATE(DeviceAddr);

#ifndef One_Bit_Switches
    if ((current_ms > SwitchPacketNextSend) || (p_auxChannelsChanged & 0xf))
    {
        SwitchPacketNextSend = current_ms + SWITCH_PACKET_SEND_INTERVAL;
        p_auxChannelsChanged = 0;

        packed_buffer[0] = PacketHeaderAddr + SWITCH_DATA_PACKET;
#if USE_RC_DATA_STRUCTS
        SwitchPacket_s *rcdata = (SwitchPacket_s *)&packed_buffer[1];
        rcdata->aux1 = (CRSF_to_UINT10(ChannelDataIn[4]) >> 6);
        rcdata->aux2 = (CRSF_to_UINT10(ChannelDataIn[5]) >> 6);
        rcdata->aux3 = (CRSF_to_UINT10(ChannelDataIn[6]) >> 6);
        rcdata->aux4 = (CRSF_to_UINT10(ChannelDataIn[7]) >> 6);
#else
        packed_buffer[1] = ((CRSF_to_UINT10(ChannelDataIn[4]) & 0b1110000000) >> 2) +
                           ((CRSF_to_UINT10(ChannelDataIn[5]) & 0b1110000000) >> 5) +
                           ((CRSF_to_UINT10(ChannelDataIn[6]) & 0b1100000000) >> 8);
        packed_buffer[2] = (CRSF_to_UINT10(ChannelDataIn[6]) & 0b0010000000) +
                           ((CRSF_to_UINT10(ChannelDataIn[7]) & 0b1110000000) >> 3);
#endif
        packed_buffer[3] = packed_buffer[1];
        packed_buffer[4] = packed_buffer[2];
        packed_buffer[5] = Radio.NonceTX;
        packed_buffer[6] = FHSSgetCurrIndex();
    }
    else // else we just have regular channel data which we send as 8 + 2 bits
#endif // !One_Bit_Switches
    {
        packed_buffer[0] = PacketHeaderAddr + RC_DATA_PACKET;

#if USE_RC_DATA_STRUCTS
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
#else
        packed_buffer[1] = ((ChannelDataIn[0]) >> 3);
        packed_buffer[2] = ((ChannelDataIn[1]) >> 3);
        packed_buffer[3] = ((ChannelDataIn[2]) >> 3);
        packed_buffer[4] = ((ChannelDataIn[3]) >> 3);
        packed_buffer[5] = ((ChannelDataIn[0] & 0b111) << 5) +
                           ((ChannelDataIn[1] & 0b111) << 2) +
                           ((ChannelDataIn[2] & 0b110) >> 1);
        packed_buffer[6] = ((ChannelDataIn[2] & 0b001) << 7) +
                           ((ChannelDataIn[3] & 0b111) << 4); // 4 bits left over for something else?
#ifdef One_Bit_Switches
        packed_buffer[6] += CRSF_to_BIT(ChannelDataIn[4]) << 3;
        packed_buffer[6] += CRSF_to_BIT(ChannelDataIn[5]) << 2;
        packed_buffer[6] += CRSF_to_BIT(ChannelDataIn[6]) << 1;
        packed_buffer[6] += CRSF_to_BIT(ChannelDataIn[7]) << 0;
#endif
#endif
    }
}

//void  UnpackChannelData_11bit()
void RcChannels::channels_extract(volatile uint8_t const *const input,
                                  crsf_channels_t &PackedRCdataOut)
{
    uint8_t type = input[0] & 0b11;
    if (type == RC_DATA_PACKET)
    {
#if USE_RC_DATA_STRUCTS
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
#else
        PackedRCdataOut.ch0 = (input[1] << 3) + ((input[5] & 0b11100000) >> 5);
        PackedRCdataOut.ch1 = (input[2] << 3) + ((input[5] & 0b00011100) >> 2);
        PackedRCdataOut.ch2 = (input[3] << 3) + ((input[5] & 0b00000011) << 1) + (input[6] & 0b10000000 >> 7);
        PackedRCdataOut.ch3 = (input[4] << 3) + ((input[6] & 0b01110000) >> 4);
#ifdef One_Bit_Switches
        PackedRCdataOut.ch4 = BIT_to_CRSF(input[6] & 0b00001000);
        PackedRCdataOut.ch5 = BIT_to_CRSF(input[6] & 0b00000100);
        PackedRCdataOut.ch6 = BIT_to_CRSF(input[6] & 0b00000010);
        PackedRCdataOut.ch7 = BIT_to_CRSF(input[6] & 0b00000001);
#endif // One_Bit_Switches
#endif
    }
    else
    {
#if USE_RC_DATA_STRUCTS
        SwitchPacket_s *rcdata = (SwitchPacket_s *)&input[1];
        PackedRCdataOut.ch4 = SWITCH3b_to_CRSF(rcdata->aux1);
        PackedRCdataOut.ch5 = SWITCH3b_to_CRSF(rcdata->aux2);
        PackedRCdataOut.ch6 = SWITCH3b_to_CRSF(rcdata->aux3);
        PackedRCdataOut.ch7 = SWITCH3b_to_CRSF(rcdata->aux4);
#else
        PackedRCdataOut.ch4 = SWITCH3b_to_CRSF((uint16_t)(input[1] & 0b11100000) >> 5); //unpack the byte structure, each switch is stored as a possible 8 states (3 bits). we shift by 2 to translate it into the 0....1024 range like the other channel data.
        PackedRCdataOut.ch5 = SWITCH3b_to_CRSF((uint16_t)(input[1] & 0b00011100) >> 2);
        PackedRCdataOut.ch6 = SWITCH3b_to_CRSF((uint16_t)((input[1] & 0b00000011) << 1) + ((input[2] & 0b10000000) >> 7));
        PackedRCdataOut.ch7 = SWITCH3b_to_CRSF((uint16_t)((input[2] & 0b01110000) >> 4));
#endif
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

#if OPTIMIZED_SEARCH
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

#else // !OPTIMIZED_SEARCH

    // look for a changed switch
    for (i = firstSwitch; i < N_SWITCHES; i++)
    {
        if (currentSwitches[i] != sentSwitches[i])
            break;
    }

    // if we didn't find a changed switch, we get here with i==N_SWITCHES
    if (i == N_SWITCHES)
        i = p_nextSwitchIndex;

    // keep track of which switch to send next if there are no changed switches
    // during the next call.
    p_nextSwitchIndex = (i + 1) % N_SWITCHES;

    // since we are going to send i, we can put that value into the sent list.
    sentSwitches[i] = currentSwitches[i];

#endif // OPTIMIZED_SEARCH

#ifdef HYBRID_SWITCHES_8
    // for hydrid switches 0 is sent on every packet, so we can skip
    // that value for the round-robin
    if (p_nextSwitchIndex < firstSwitch)
        p_nextSwitchIndex = firstSwitch;
#endif

    return i;
}
