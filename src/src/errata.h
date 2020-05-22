#if defined(Regulatory_Domain_AU_915) || defined(Regulatory_Domain_EU_868) || defined(Regulatory_Domain_FCC_915) || defined(Regulatory_Domain_AU_433) || defined(Regulatory_Domain_EU_433)
#include "LoRaRadioLib.h"
#elif Regulatory_Domain_ISM_2400
#include "SX1280RadioLib.h"
#endif

#define REG_FREQ_ERROR_LSB 0x2a
#define REG_RSSI_WIDEBAND 0x2c
#define REG_DETECTION_OPTIMIZE 0x31
#define REG_SEQ_CONFIG_1 0x36
#define REG_TIMER2_COEF 0x3a
#define REG_DETECTION_THRESHOLD 0x37
#define REG_SYNC_WORD 0x39
#define REG_DIO_MAPPING_1 0x40

void errata()
{
    writeRegister(REG_SEQ_CONFIG_1, 0x02);
    writeRegister(REG_TIMER2_COEF, 0x7f);
}

// +  } else {
// +    writeRegister(REG_SEQ_CONFIG_1, 0x03);
// +  }