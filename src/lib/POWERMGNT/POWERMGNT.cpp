#include "POWERMGNT.h"
#include "../../src/targets.h"

#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
extern SX127xDriver Radio;
#elif Regulatory_Domain_ISM_2400
extern SX1280Driver Radio;
#endif

#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)
extern DAC TxDAC;
#endif

PowerLevels_e POWERMGNT::CurrentPower = (PowerLevels_e)DefaultPowerEnum;

PowerLevels_e POWERMGNT::incPower()
{
    if (CurrentPower < MaxPower)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower + 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::decPower()
{
    if (CurrentPower > 0)
    {
        setPower((PowerLevels_e)((uint8_t)CurrentPower - 1));
    }
    return CurrentPower;
}

PowerLevels_e POWERMGNT::currPower()
{
    return CurrentPower;
}

void POWERMGNT::init()
{


#if defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)
    Serial.println("Init DAC Driver");
#endif
#if defined(GPIO_PIN_FAN_EN) && (GPIO_PIN_FAN_EN != UNDEF_PIN)
    pinMode(GPIO_PIN_FAN_EN, OUTPUT);
#endif
#if defined(GPIO_PIN_RF_AMP_EN) && (GPIO_PIN_RF_AMP_EN != UNDEF_PIN)
    pinMode(GPIO_PIN_RF_AMP_EN, OUTPUT);
    digitalWrite(GPIO_PIN_RF_AMP_EN, HIGH);
#endif
}

void POWERMGNT::setDefaultPower()
{
    setPower((PowerLevels_e)DefaultPowerEnum);
}

PowerLevels_e POWERMGNT::setPower(PowerLevels_e Power)
{
    if (Power > MaxPower)
    {
        Power = (PowerLevels_e)MaxPower;
    }

#if defined(TARGET_TX_ESP32_SX1280_V1) || defined(TARGET_RX_ESP8266_SX1280_V1)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(8);
        break;
    case PWR_25mW:
    default:
        Radio.SetOutputPower(13);
        Power = PWR_25mW;
        break;
    }
#elif defined(TARGET_TX_GHOST)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(0);
        break;
    case PWR_25mW:
        Radio.SetOutputPower(4);
        break;
    case PWR_100mW:
        Radio.SetOutputPower(10);
        break;
    case PWR_250mW:
        Radio.SetOutputPower(13);
        break;
    case PWR_50mW:
    default:
        Power = PWR_50mW;
        Radio.SetOutputPower(7);
        break;
    }
#elif defined(TARGET_R9M_TX) || defined(TARGET_TX_ES915TX)
    Radio.SetOutputPower(0b0000);
    TxDAC.setPower((DAC_PWR_)Power);
#ifdef GPIO_PIN_FAN_EN
    (Power >= PWR_250mW) ? digitalWrite(GPIO_PIN_FAN_EN, HIGH) : digitalWrite(GPIO_PIN_FAN_EN, LOW);
#endif
#elif defined(TARGET_R9M_LITE_PRO_TX)
    Radio.SetOutputPower(0b0000);
#elif defined(TARGET_100mW_MODULE) || defined(TARGET_R9M_LITE_TX)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(0b1000);
        CurrentPower = PWR_10mW;
        break;
    case PWR_25mW:
        Radio.SetOutputPower(0b1100);
        CurrentPower = PWR_25mW;
        break;
    case PWR_50mW:
    default:
        Power = PWR_50mW;
        Radio.SetOutputPower(0b1111); //15
        break;
    }
#elif defined(TARGET_1000mW_MODULE)
    switch (Power)
    {
    case PWR_100mW:
        Radio.SetOutputPower(0b0101);
        break;
    case PWR_250mW:
        Radio.SetOutputPower(0b1000);
        break;
    case PWR_500mW:
        Radio.SetOutputPower(0b1100);
        break;
    case PWR_1000mW:
        Radio.SetOutputPower(0b1111);
        break;
    case PWR_50mW:
    default:
        Radio.SetOutputPower(0b0010);
        Power = PWR_50mW;
        break;
    }
#elif defined(TARGET_TX_ESP32_E28_SX1280_V1)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(-15);
        break;
    case PWR_25mW:
        Radio.SetOutputPower(-11);
        break;
    case PWR_50mW:
        Radio.SetOutputPower(-8);
        break;
    case PWR_100mW:
        Radio.SetOutputPower(-5);
        break;
    case PWR_250mW:
        Radio.SetOutputPower(-1);
        break;
    default:
        Power = PWR_50mW;
        Radio.SetOutputPower(-8);
        break;
    }
#elif defined(TARGET_TX_ESP32_LORA1280F27)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(-4);
        break;
    case PWR_25mW:
        Radio.SetOutputPower(0);
        break;
    case PWR_50mW:
        Radio.SetOutputPower(3);
        break;
    case PWR_100mW:
        Radio.SetOutputPower(6);
        break;
    case PWR_250mW:
        Radio.SetOutputPower(12);
        break;
    default:
        Power = PWR_50mW;
        Radio.SetOutputPower(3);
        break;
    }
#elif defined(TARGET_TX_FM30)
    switch (Power)
    {
    case PWR_10mW:
        Radio.SetOutputPower(-15); // ~10.5mW
        break;
    case PWR_25mW:
        Radio.SetOutputPower(-11); // ~26mW
        break;
    case PWR_100mW:
        Radio.SetOutputPower(-1);  // ~99mW
        break;
    case PWR_250mW:
        // The original FM30 can somehow put out +22dBm but the 2431L max input
        // is +6dBm, and even when SetOutputPower(13) you still only get 150mW
        Radio.SetOutputPower(6);  // ~150mW
        break;
    case PWR_50mW:
    default:
        Power = PWR_50mW;
        Radio.SetOutputPower(-7); // -7=~55mW, -8=46mW
        break;
    }
#else
#error "[ERROR] Unknown power management!"
#endif
    CurrentPower = Power;
    return Power;
}
