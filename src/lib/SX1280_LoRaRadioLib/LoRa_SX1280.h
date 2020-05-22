#pragma once

#include <Arduino.h>

#include "../../src/targets.h"
#include "LoRa_SX1280_Regs.h"
#include "LoRA_SX1280_hal.h"

void ICACHE_RAM_ATTR TXnbISR();

enum InterruptAssignment_
{
    NONE,
    RX_DONE,
    TX_DONE
};

class SX1280Driver
{

public:
    ///////Callback Function Pointers/////
    static void ICACHE_RAM_ATTR nullCallback(void){};

    void (*RXdoneCallback1)() = &nullCallback; //function pointer for callback
    void (*RXdoneCallback2)() = &nullCallback; //function pointer for callback

    void (*TXdoneCallback1)() = &nullCallback; //function pointer for callback
    void (*TXdoneCallback2)() = &nullCallback; //function pointer for callback
    void (*TXdoneCallback3)() = &nullCallback; //function pointer for callback
    void (*TXdoneCallback4)() = &nullCallback; //function pointer for callback

    static void (*TXtimeout)(); //function pointer for callback
    static void (*RXtimeout)(); //function pointer for callback

    InterruptAssignment_ InterruptAssignment = NONE;
    /////////////////////////////

    ///////////Radio Variables////////
    static uint8_t TXdataBuffer[256];
    static uint8_t RXdataBuffer[256];

    static volatile uint8_t TXbuffLen;
    static volatile uint8_t RXbuffLen;

    //static volatile bool headerExplMode;

    static uint32_t currFreq;
    static uint8_t _syncWord;

    static SX1280_RadioLoRaBandwidths_t currBW;
    static SX1280_RadioLoRaSpreadingFactors_t currSF;
    static SX1280_RadioLoRaCodingRates_t currCR;
    static SX1280_RadioOperatingModes_t currOpmode;

    // static uint8_t currPWR;
    // static uint8_t maxPWR;

    ///////////////////////////////////

    /////////////Packet Stats//////////
    static int8_t LastPacketRSSI;
    static int8_t LastPacketSNR;
    static float PacketLossRate;
    static volatile uint8_t NonceTX;
    static volatile uint8_t NonceRX;
    static uint32_t TotalTime;
    static uint32_t TimeOnAir;
    static uint32_t TXstartMicros;
    static uint32_t TXspiTime;
    static uint32_t HeadRoom;
    static uint32_t TXdoneMicros;
    /////////////////////////////////

    //// Local Variables //// Copy of values for SPI speed optimisation
    static uint8_t CURR_REG_PAYLOAD_LENGTH;
    static uint8_t CURR_REG_DIO_MAPPING_1;
    static uint8_t CURR_REG_FIFO_ADDR_PTR;

    ////////////////Configuration Functions/////////////
    SX1280Driver();
    static SX1280Driver *instance;
    void Begin();
    void setMode(SX1280_RadioOperatingModes_t OPmode);
    void configModParams(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr);
    void setPacketParams(SX1280_RadioPreambleLengths_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType, uint8_t PayloadLength, SX1280_RadioLoRaCrcModes_t crc, SX1280_RadioLoRaIQModes_t InvertIQ);
    void SetFrequency(uint32_t freq);
    void setFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr);

    int32_t GetFrequencyError();

    void ICACHE_RAM_ATTR TXnb(volatile uint8_t *data, uint8_t length);
    static void ICACHE_RAM_ATTR TXnbISR(); //ISR for non-blocking TX routine

    void ICACHE_RAM_ATTR RXnb();
    static void ICACHE_RAM_ATTR RXnbISR(); //ISR for non-blocking RC routine

    void ICACHE_RAM_ATTR ClearIrqStatus(uint16_t irqMask);

    void SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask);

    // uint8_t SetBandwidth(SX1280_RadioLoRaBandwidths_t bw);
    // uint32_t getCurrBandwidth();
    // uint8_t SetOutputPower(uint8_t Power);
    // uint8_t SetPreambleLength(uint8_t PreambleLen);
    // uint8_t SetSpreadingFactor(SX1280_RadioLoRaSpreadingFactors_t sf);
    // uint8_t SetCodingRate(SX1280_RadioLoRaCodingRates_t cr);

private:
};