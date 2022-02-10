#pragma once

#include "targets.h"
#include "SX1280_Regs.h"
#include "SX1280_hal.h"

class SX1280Driver
{
public:
    ///////Callback Function Pointers/////
    static void ICACHE_RAM_ATTR nullCallback(void);

    void (*RXdoneCallback)() = &nullCallback; //function pointer for callback
    void (*TXdoneCallback)() = &nullCallback; //function pointer for callback

    static void (*TXtimeout)(); //function pointer for callback
    static void (*RXtimeout)(); //function pointer for callback

    ///////////Radio Variables////////
    #define TXRXBuffSize 16
    volatile uint8_t TXdataBuffer[TXRXBuffSize];
    volatile uint8_t RXdataBuffer[TXRXBuffSize];

    uint8_t PayloadLength = 8; // Dummy default value which is overwritten during setup.

    static uint8_t _syncWord;

    SX1280_RadioLoRaBandwidths_t currBW = SX1280_LORA_BW_0800;
    SX1280_RadioLoRaSpreadingFactors_t currSF = SX1280_LORA_SF6;
    SX1280_RadioLoRaCodingRates_t currCR = SX1280_LORA_CR_4_7;
    SX1280_RadioLoRaPacketLengthsModes_t currHeaderMode;
    uint32_t currFreq = 2400000000;
    SX1280_RadioOperatingModes_t currOpmode = SX1280_MODE_SLEEP;
    bool IQinverted = false;
    uint16_t timeout = 0xFFFF;

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
    void SetTxIdleMode() { SetMode(SX1280_MODE_FS); }; // set Idle mode used when switching from RX to TX
    void Config(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr, uint32_t freq, uint8_t PreambleLength, bool InvertIQ, uint8_t PayloadLength, uint32_t interval);
    void ConfigLoRaModParams(SX1280_RadioLoRaBandwidths_t bw, SX1280_RadioLoRaSpreadingFactors_t sf, SX1280_RadioLoRaCodingRates_t cr);
    void SetPacketParams(uint8_t PreambleLength, SX1280_RadioLoRaPacketLengthsModes_t HeaderType, uint8_t PayloadLength, SX1280_RadioLoRaCrcModes_t crc, SX1280_RadioLoRaIQModes_t InvertIQ);
    void ICACHE_RAM_ATTR SetFrequencyHz(uint32_t freq);
    void ICACHE_RAM_ATTR SetFrequencyReg(uint32_t freq);
    void ICACHE_RAM_ATTR SetFIFOaddr(uint8_t txBaseAddr, uint8_t rxBaseAddr);
    void SetRxTimeoutUs(uint32_t interval);
    void SetOutputPower(int8_t power);
    void SetOutputPowerMax() { SetOutputPower(13); };

    bool ICACHE_RAM_ATTR GetFrequencyErrorbool();
    bool FrequencyErrorAvailable() const
        { return (currHeaderMode == SX1280_LORA_PACKET_VARIABLE_LENGTH) && (LastPacketSNR > 0); }

    void TXnb();
    void RXnb();

    uint16_t GetIrqStatus();
    void ClearIrqStatus(uint16_t irqMask);

    void GetStatus();

    void SetDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask);

    uint8_t GetRxBufferAddr();
    void GetLastPacketStats();

private:
    static void ICACHE_RAM_ATTR IsrCallback();
    void RXnbISR(); // ISR for non-blocking RX routine
    void TXnbISR(); // ISR for non-blocking TX routine
};
