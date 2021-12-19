#include "targets.h"
#include "common.h"
#include "device.h"
#include "devVTXSPI.h"

#if defined(GPIO_PIN_SPI_VTX_NSS) && (GPIO_PIN_SPI_VTX_NSS != UNDEF_PIN)
#include "logging.h"
#include <SPI.h>

#define SYNTHESIZER_REGISTER_A                  0x00
#define SYNTHESIZER_REGISTER_B                  0x01
#define SYNTHESIZER_REGISTER_C                  0x02
#define RF_VCO_DFC_CONTROL_REGISTER             0x03
#define VCO_CONTROL_REGISTER                    0x04
#define VCO_CONTROL_REGISTER_CONT               0x05
#define AUDIO_MODULATOR_CONTROL_REGISTER        0x06
#define PRE_DRIVER_AND_PA_CONTROL_REGISTER      0x07
#define STATE_REGISTER                          0x0F

#define SYNTH_REG_A_DEFAULT                     0x0190

#define POWER_AMP_ON                            0b00000100111110111111
#define POWER_AMP_OFF                           0x00
#define MAX_PWM                                 4095

#define READ_BIT                                0x00
#define WRITE_BIT                               0x01

#define RTC6705_BOOT_DELAY                      350
#define RTC6705_PLL_SETTLE_TIME_MS              500 // ms - after changing frequency turn amps back on after this time for clean switching
#define VTX_POWER_INTERVAL_MS                   100

#define BUF_PACKET_SIZE                         4 // 25b packet in 4 bytes

uint8_t vtxSPIBandChannelIdx = 255;
uint8_t vtxSPIBandChannelIdxCurrent = 255;
uint8_t vtxSPIPowerIdx = 0;
uint8_t vtxSPIPitmode = 1;
uint8_t rtc6705PowerAmpState = 0;
uint16_t vtxSPIPWM = MAX_PWM;

const uint16_t freqTable[48] = {
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // A
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // B
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // E
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // F
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917, // R
    5333, 5373, 5413, 5453, 5493, 5533, 5573, 5613  // L
};

static void rtc6705WriteRegister(uint32_t regData)
{
    uint8_t buf[BUF_PACKET_SIZE];
    memcpy (buf, (byte *) &regData, BUF_PACKET_SIZE);

    SPI.setFrequency(1000000); // Only for testing
    SPI.setBitOrder(LSBFIRST);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, LOW);
    SPI.transfer(buf, BUF_PACKET_SIZE);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, HIGH);
    SPI.setBitOrder(MSBFIRST);
    SPI.setFrequency(10000000); // Only for testing
}

static void rtc6705ResetSynthRegA()
{
  uint32_t regData = SYNTHESIZER_REGISTER_A | (WRITE_BIT << 4) | (SYNTH_REG_A_DEFAULT << 5);
  rtc6705WriteRegister(regData);
}

static void rtc6705SetFrequency(uint32_t freq)
{
    VTxOutputMinimum(); // Set power to zero for clear channel switching
  
    uint32_t f = 25 * freq;
    uint32_t SYN_RF_N_REG = f / 64;
    uint32_t SYN_RF_A_REG = f % 64;

    uint32_t regData = SYNTHESIZER_REGISTER_B | (WRITE_BIT << 4) | (SYN_RF_A_REG << 5) | (SYN_RF_N_REG << 12);

    rtc6705WriteRegister(regData);
}

static void rtc6705SetFrequencyByIdx(uint8_t idx)
{
    rtc6705SetFrequency((uint32_t)freqTable[idx]);
}

void rtc6705PowerAmpOff(void)
{
    if (rtc6705PowerAmpState)
    {
        uint32_t regData = PRE_DRIVER_AND_PA_CONTROL_REGISTER | (WRITE_BIT << 4) | (POWER_AMP_OFF << 5);
        rtc6705WriteRegister(regData);
        rtc6705PowerAmpState = 0;
    }
}

void rtc6705PowerAmpOn(void)
{
    if (!rtc6705PowerAmpState)
    {
        uint32_t regData = PRE_DRIVER_AND_PA_CONTROL_REGISTER | (WRITE_BIT << 4) | (POWER_AMP_ON << 5);
        rtc6705WriteRegister(regData);
        rtc6705PowerAmpState = 1;
    }
}

void VTxOutputMinimum(void)
{
    // vtxSPIPWM = MAX_PWM;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
    rtc6705PowerAmpOff();
}

void VTxOutputIncrease(void)
{
    // if (vtxSPIPWM > 0) vtxSPIPWM--;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
}

void VTxOutputDecrease(void)
{
    // if (vtxSPIPWM < MAX_PWM) vtxSPIPWM++;
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);
}

static void checkOutputPower()
{
    if (vtxSPIPitmode)
    {
        INFOLN("Pitmode On...");

        VTxOutputMinimum();
    }
    else
    {
        INFOLN("Pitmode Off...");

        rtc6705PowerAmpOn();
    
        // uint16_t Vpd = analogRead(GPIO_PIN_RF_AMP_VPD); // 0 - 1023 and max input 1.0V

        // uint16_t VpdSetPoint = 500; // made up number

        // if (Vpd < VpdSetPoint)
        // {
        //     VTxOutputIncrease();
        // }
        // else if (Vpd > VpdSetPoint)
        // {
        //     VTxOutputDecrease();
        // }
    }
}

static void initialize()
{
    pinMode(GPIO_PIN_SPI_VTX_NSS, OUTPUT);
    digitalWrite(GPIO_PIN_SPI_VTX_NSS, HIGH);
    
    // pinMode(GPIO_PIN_RF_AMP_PWM, OUTPUT);
    // analogWriteFreq(10000); // 10kHz
    // analogWriteResolution(14); // 0 - 4095
    // analogWrite(GPIO_PIN_RF_AMP_PWM, vtxSPIPWM);

    delay(RTC6705_BOOT_DELAY);
}

static int start()
{
    return VTX_POWER_INTERVAL_MS;
}

static int event()
{
    if (vtxSPIBandChannelIdxCurrent != vtxSPIBandChannelIdx)
    {
        return DURATION_IMMEDIATELY;
    }

    return DURATION_IGNORE;
}

static int timeout()
{
    if (vtxSPIBandChannelIdxCurrent != vtxSPIBandChannelIdx)
    {        
        rtc6705ResetSynthRegA();
        rtc6705SetFrequencyByIdx(vtxSPIBandChannelIdx);
        vtxSPIBandChannelIdxCurrent = vtxSPIBandChannelIdx;

        INFOLN("VTx set frequency...");

        return RTC6705_PLL_SETTLE_TIME_MS;
    }
    else
    {
        checkOutputPower();
        
        INFOLN("VTx check output...");

        return VTX_POWER_INTERVAL_MS;
    }
}

device_t VTxSPI_device = {
    .initialize = initialize,
    .start = start,
    .event = event,
    .timeout = timeout
};

#endif