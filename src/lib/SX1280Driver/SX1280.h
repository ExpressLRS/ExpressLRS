#pragma once

#include "targets.h"
#include "SX1280_Regs.h"
#include "SX1280_hal.h"

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
    static void ICACHE_RAM_ATTR nullCallback(void);

    void (*RXdoneCallback)() = &nullCallback; //function pointer for callback
    void (*TXdoneCallback)() = &nullCallback; //function pointer for callback

    static void (*TXtimeout)(); //function pointer for callback
    static void (*RXtimeout)(); //function pointer for callback

    InterruptAssignment_ InterruptAssignment = NONE;
    /////////////////////////////

    ///////////Radio Variables////////
    #define TXRXBuffSize 8 

    volatile uint8_t TXdataBuffer[TXRXBuffSize]; // ELRS uses max of 8 bytes
    volatile uint8_t RXdataBuffer[TXRXBuffSize];

    static uint8_t _syncWord;

    SX1280_RadioLoRaBandwidths_t currBW = SX1280_LORA_BW_0800;
    SX1280_RadioLoRaSpreadingFactors_t currSF = SX1280_LORA_SF6;
    SX1280_RadioLoRaCodingRates_t currCR = SX1280_LORA_CR_4_7;
    uint32_t currFreq = 2400000000;
    SX1280_RadioOperatingModes_t currOpmode = SX1280_MODE_SLEEP;
    bool IQinverted = false;

    // static uint8_t currPWR;
    // static uint8_t maxPWR;

    ///////////////////////////////////

    /////////////Packet Stats//////////
    int8_t LastPacketRSSI = 0;
    int8_t LastPacketSNR = 0;
    volatile uint8_t NonceTX = 0;
    volatile uint8_t NonceRX = 0;
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
    bool Begin();
    void End();
    void SetMode(SX1280_RadioOperatingModes_t OPmode);
    void Config(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr, uint32_t freq, uint8_t PreambleLength, bool InvertIQ);
    void ConfigLoRaModParams(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr);
    void SetPacketParams(uint8_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType, uint8_t PayloadLength, SX1280_RadioLoRaCrcModes_t crc, SX1280_RadioLoRaIQModes_t InvertIQ);
    void ICACHE_RAM_ATTR SetFrequencyHz(uint32_t freq);
    void ICACHE_RAM_ATTR SetFrequencyReg(uint32_t freq);
    void ICACHE_RAM_ATTR SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr);
    void SetOutputPower(int8_t power);

    int32_t ICACHE_RAM_ATTR GetFrequencyError();
    float GetNoiseFloorInRange(uint32_t startFreq, uint32_t endFreq, uint32_t step);

    static void TXnb(volatile uint8_t *data, uint8_t length);
    static void TXnbISR(); //ISR for non-blocking TX routine

    static void RXnb();
    static void RXnbISR(); //ISR for non-blocking RC routine

    void  ClearIrqStatus(uint16_t irqMask);

    void GetStatus();
    int8_t GetRSSIinst();

    void SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask);
    
    bool GetFrequencyErrorbool();
    uint8_t GetRxBufferAddr();
    void GetLastPacketStats();

private:
};
