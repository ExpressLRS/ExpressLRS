#include "targets.h"
#include "debug.h"
#include "common.h"
#include <Arduino.h>
//#include "LED.h"
#include <EEPROM.h>

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#endif
#include "esp_task_wdt.h"

#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"

void feedTheDog(void)
{
    // feed dog 0
    TIMERG0.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
    TIMERG0.wdt_feed = 1;                       // feed dog
    TIMERG0.wdt_wprotect = 0;                   // write protect
    // feed dog 1
    TIMERG1.wdt_wprotect = TIMG_WDT_WKEY_VALUE; // write enable
    TIMERG1.wdt_feed = 1;                       // feed dog
    TIMERG1.wdt_wprotect = 0;                   // write protect
}

/******************* CONFIG *********************/
int8_t platform_config_load(struct platform_config &config)
{
#if STORE_TO_FLASH
    struct platform_config temp;

    temp.key = EEPROM.readUInt(offsetof(struct platform_config, key));
    temp.mode = EEPROM.readUInt(offsetof(struct platform_config, mode));
    temp.power = EEPROM.readUInt(offsetof(struct platform_config, power));
    temp.tlm = EEPROM.readUInt(offsetof(struct platform_config, tlm));

    if (temp.key == ELRS_EEPROM_KEY)
    {
        /* load ok, copy values */
        config.key = temp.key;
        config.mode = temp.mode;
        config.power = temp.power;
        config.tlm = temp.tlm;
        return 0;
    }
    return -1;
#else
    config.key = ELRS_EEPROM_KEY;
    return 0;
#endif
}

int8_t platform_config_save(struct platform_config &config)
{
    if (config.key != ELRS_EEPROM_KEY)
        return -1;
#if STORE_TO_FLASH
    EEPROM.writeUInt(offsetof(struct platform_config, key), config.key);
    EEPROM.writeUInt(offsetof(struct platform_config, mode), config.mode);
    EEPROM.writeUInt(offsetof(struct platform_config, power), config.power);
    EEPROM.writeUInt(offsetof(struct platform_config, tlm), config.tlm);
    return EEPROM.commit() ? 0 : -1;
#else
    return 0;
#endif
}

/******************* SETUP *********************/
void platform_setup(void)
{
    disableCore0WDT();
    disableCore1WDT();

    EEPROM.begin(sizeof(struct platform_config));

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3_LEGACY
    pinMode(RC_SIGNAL_PULLDOWN, INPUT_PULLDOWN);
    pinMode(GPIO_PIN_BUTTON, INPUT_PULLUP);
#endif

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.begin(115200);
#endif

#ifdef TARGET_EXPRESSLRS_PCB_TX_V3
    //WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); //disable brownout detector

    //strip.Begin();

    uint8_t baseMac[6];
    // Get base mac address
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    // Print base mac address
    // This should be copied to common.h and is used to generate a unique hop sequence, DeviceAddr, and CRC.
    // UID[0..2] are OUI (organisationally unique identifier) and are not ESP32 unique.  Do not use!
    DEBUG_PRINTLN("");
    DEBUG_PRINTLN("Copy the below line into common.h.");
    DEBUG_PRINT("uint8_t UID[6] = {");
    DEBUG_PRINT(baseMac[0]);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(baseMac[1]);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(baseMac[2]);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(baseMac[3]);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(baseMac[4]);
    DEBUG_PRINT(", ");
    DEBUG_PRINT(baseMac[5]);
    DEBUG_PRINTLN("};");
    DEBUG_PRINTLN("");
#endif
}

void platform_mode_notify(void)
{
}

void platform_loop(int state)
{
    (void)state;
}

void platform_connection_state(int state)
{
    (void)state;
}

void platform_set_led(bool state)
{
}

void platform_restart(void)
{
    ESP.restart();
}

void platform_wd_feed(void)
{
    //esp_task_wdt_reset(); // make sure the WD is feeded
    feedTheDog();
    yield();
}
