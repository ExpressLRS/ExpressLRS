#include "targets.h"
#include "debug.h"
#include "common.h"
#include <Arduino.h>
//#include "LED.h"

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

void platform_setup(void)
{
#ifdef TARGET_EXPRESSLRS_PCB_TX_V3_LEGACY
    pinMode(RC_SIGNAL_PULLDOWN, INPUT_PULLDOWN);
    pinMode(GPIO_PIN_BUTTON, INPUT_PULLUP);
#endif

#ifdef DEBUG_SERIAL
    DEBUG_SERIAL.begin(250000);
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

void platform_loop(connectionState_e state)
{
    (void)state;
    esp_task_wdt_reset(); // make sure the WD is feeded
}

void platform_connection_state(connectionState_e state)
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
