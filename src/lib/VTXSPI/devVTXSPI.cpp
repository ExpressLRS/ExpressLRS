#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_SPI_VTX_NSS) && (GPIO_PIN_SPI_VTX_NSS != UNDEF_PIN)
#include "logging.h"
#include <SPI.h>

#define SYNTHESIZERREGISTERA            0x00
#define SYNTHESIZERREGISTERB            0x01
#define SYNTHESIZERREGISTERC            0x02
#define RFVCODFCCONTROLREGISTER         0x03
#define VCOCONTROLREGISTER              0x04
#define VCOCONTROLREGISTERCONT          0x05
#define AUDIOMODULATORCONTROLREGISTER   0x06
#define PREDRIVERANDPACONTROLREGISTER   0x07
#define STATEREGISTER                   0x0F
#define READ_BIT                        0x00
#define WRITE_BIT                       0x01
#define POWER_AMP_ON                    0b10011111011111100000 
#define PLL_SETTLE_TIME_MS              500 // ms - after changing frequency turn amps back on after this time for clean switching
#define VTX_POWER_INTERVAL_MS           5
#define BUF_PACKET_SIZE                 4 // 25b packet in 4 bytes

uint8_t vtxSPIBandChannelIdx = 255;
uint8_t vtxSPIBandChannelIdxCurrent = 255;
uint8_t vtxSPIPowerIdx = 0;
uint8_t vtxSPIPitmode = 1;
uint8_t rtc6705PowerAmpState = 0;
uint16_t vtxSPIPWM = 4095;

const uint16_t channelFreqTable[48] = {
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // A
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // B
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // E
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // F
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, // R
    5333, 5373, 5413, 5453, 5493, 5533, 5573, 5613  // L
};

static void rtc6705WriteRegister(uint32_t data)
{
    uint8_t buf[BUF_PACKET_SIZE];
    memcpy (buf, (byte *) &data, BUF_PACKET_SIZE);

    SPI.setBitOrder(LSBFIRST);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, LOW);
    SPI.writeBytes(buf, BUF_PACKET_SIZE);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, HIGH);
    SPI.setBitOrder(MSBFIRST);
}

void rtc6705ResetState(void)
{
    // uint32_t regData = STATEREGISTER | (WRITE_BIT << 4);
    // rtc6705WriteRegister(regData);
}

void rtc6705PowerAmpOff(void)
{
    // if (rtc6705PowerAmpState)
    // {
    //     uint32_t regData = PREDRIVERANDPACONTROLREGISTER | (WRITE_BIT << 4);
    //     rtc6705WriteRegister(regData);
    //     rtc6705PowerAmpState = 0;
    // }
}

void rtc6705PowerAmpOn(void)
{
    // if (!rtc6705PowerAmpState)
    // {
    //     uint32_t regData = PREDRIVERANDPACONTROLREGISTER | (WRITE_BIT << 4) | POWER_AMP_ON;
    //     rtc6705WriteRegister(regData);
    //     rtc6705PowerAmpState = 1;
    // }
}

void VTxOutputMinimum(void)
{
    // vtxSPIPWM = 4095;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
    // rtc6705PowerAmpOff();
}

void VTxOutputIncrease(void)
{
    // if (vtxSPIPWM > 0) vtxSPIPWM--;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
}

void VTxOutputDecrease(void)
{
    // if (vtxSPIPWM < 4095) vtxSPIPWM++;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
}

static void rtc6705SetFrequency()
{
    VTxOutputMinimum(); // Set power to zero for clear channel switching
  
    uint32_t freq = 1000 * channelFreqTable[vtxSPIBandChannelIdx];
    freq /= 40;
    uint32_t SYN_RF_N_REG = freq / 64;
    uint32_t SYN_RF_A_REG = freq % 64;

    uint32_t regData = SYNTHESIZERREGISTERB | (WRITE_BIT << 4) | (SYN_RF_A_REG << 5) | (SYN_RF_N_REG << 12);

    rtc6705WriteRegister(regData);
}

static void checkOutputPower()
{
    // if (vtxSPIPitmode)
    // {
    //     // INFOLN("Pitmode On...");

    //     VTxOutputMinimum();
    // }
    // else
    // {
    //     // INFOLN("Pitmode Off...");

    //    rtc6705PowerAmpOn();
    
    //     uint16_t Vpd = analogRead(GPIO_PIN_RF_AMP_VPD); // 0 - 1023 and max input 1.0V

    //     uint16_t VpdSetPoint = 500; // made up number

    //     if (Vpd < VpdSetPoint)
    //     {
    //         VTxOutputIncrease();
    //     }
    //     else if (Vpd > VpdSetPoint)
    //     {
    //         VTxOutputDecrease();
    //     }
    // }
}

static void initialize()
{
    pinMode(GPIO_PIN_SPI_VTX_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, HIGH);
    
    // pinMode(GPIO_PIN_RF_AMP_PWM, OUTPUT);
    // analogWriteFreq(10000); // 10kHz
    // analogWriteResolution(14); // 0 - 4095
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);

    // rtc6705ResetState();
}

static int start()
{
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (vtxSPIBandChannelIdxCurrent != vtxSPIBandChannelIdx)
    {
        SPI.setFrequency(1000000); // 10M SPI needs testing on the RTC6705
        rtc6705SetFrequency();
        SPI.setFrequency(10000000);
        vtxSPIBandChannelIdxCurrent = vtxSPIBandChannelIdx;

        INFOLN("VTx set frequency...");

        return PLL_SETTLE_TIME_MS;
    }
    else
    {
        // checkOutputPower();
        
        // INFOLN("VTx check output...");

        return VTX_POWER_INTERVAL_MS;
    }
}

device_t VTxSPI_device = {
    .initialize = initialize,
    .start = start,
    .event = NULL,
    .timeout = timeout
};

#endif