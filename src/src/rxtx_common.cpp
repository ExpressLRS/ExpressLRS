#include "targets.h"
#include "common.h"
#include "config.h"
#include "logging.h"

#include <functional>
#include <Wire.h>
#include "SPI.h"

static const int maxDeferredFunctions = 3;

struct deferred_t {
    unsigned long started;
    unsigned long timeout;
    std::function<void()> function;
};

static deferred_t deferred[maxDeferredFunctions] = {
    {0, 0, nullptr},
    {0, 0, nullptr},
    {0, 0, nullptr},
};

boolean i2c_enabled = false;
static unsigned long rebootTime_Ms = 0;
// NEW SPI DEVICES Support
boolean spi_enabled = false;
SPIClass _spi;

static void setupWire()
{
    int gpio_scl = GPIO_PIN_SCL;
    int gpio_sda = GPIO_PIN_SDA;

#if defined(TARGET_RX)
    for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
    {
        auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
        auto pwm = config.GetPwmChannel(ch);
        // if the PWM pin is nominated as SDA or SCL, and it's not configured for I2C then undef the pins
        if ((pin == GPIO_PIN_SCL && pwm->val.mode != somSCL) || (pin == GPIO_PIN_SDA && pwm->val.mode != somSDA))
        {
            gpio_scl = UNDEF_PIN;
            gpio_sda = UNDEF_PIN;
            break;
        }
        // If I2C pins are not defined in the hardware, then look for configured I2C
        if (GPIO_PIN_SCL == UNDEF_PIN && pwm->val.mode == somSCL)
        {
            gpio_scl = pin;
        }
        if (GPIO_PIN_SCL == UNDEF_PIN && pwm->val.mode == somSDA)
        {
            gpio_sda = pin;
        }
    }
#endif
    if(gpio_sda != UNDEF_PIN && gpio_scl != UNDEF_PIN)
    {
        DBGLN("Starting wire on SCL %d, SDA %d", gpio_scl, gpio_sda);
        // ESP hopes to get Wire::begin(int, int)
        // ESP32 hopes to get Wire::begin(int = -1, int = -1, uint32 = 0)
        Wire.begin(gpio_sda, gpio_scl);
        Wire.setClock(400000);
        i2c_enabled = true;
    }
}

void setupSPI() 
{
#if defined(PLATFORM_ESP32) && defined(TARGET_RX)

    //spi dev
    int gpio_sck = GPIO_PIN_SPI_SCK;
    int gpio_miso = GPIO_PIN_SPI_MISO;
    int gpio_nss = GPIO_PIN_SPI_NSS;
    int gpio_mosi = GPIO_PIN_SPI_MOSI;
    int gpio_rst = GPIO_PIN_SPI_RST;
    int gpio_buys = GPIO_PIN_SPI_BUSY;
    int gpio_int = GPIO_PIN_SPI_INT;
    DBGLN("SPI: gpio_sck :%d ,gpio_miso: %d , gpio_mosi: %d ,gpio_nss: %d",gpio_sck,gpio_miso,gpio_mosi,gpio_nss);

    for (int ch = 0 ; ch < GPIO_PIN_PWM_OUTPUTS_COUNT ; ++ch)
    {
        auto pin = GPIO_PIN_PWM_OUTPUTS[ch];
        auto pwm = config.GetPwmChannel(ch);

        // Checkig for conflict on PWM pins
        bool conflict = false;

        if ((GPIO_PIN_SPI_SCK  != UNDEF_PIN && pin == GPIO_PIN_SPI_SCK  && pwm->val.mode != somSCK)  ||
            (GPIO_PIN_SPI_MOSI != UNDEF_PIN && pin == GPIO_PIN_SPI_MOSI && pwm->val.mode != somMOSI) ||
            (GPIO_PIN_SPI_MISO != UNDEF_PIN && pin == GPIO_PIN_SPI_MISO && pwm->val.mode != somMISO) ||
            (GPIO_PIN_SPI_NSS  != UNDEF_PIN && pin == GPIO_PIN_SPI_NSS  && pwm->val.mode != somNSS)  ||
            (GPIO_PIN_SPI_INT  != UNDEF_PIN && pin == GPIO_PIN_SPI_INT  && pwm->val.mode != somINT))
        {
            conflict = true;
        }

        if (conflict) {
            gpio_sck   = UNDEF_PIN;
            gpio_mosi  = UNDEF_PIN;
            gpio_miso  = UNDEF_PIN;
            gpio_nss   = UNDEF_PIN;
            gpio_int   = UNDEF_PIN;
            DBGLN("spi pin conflict");
            break;
        }

        if (GPIO_PIN_SPI_SCK == UNDEF_PIN && pwm->val.mode == somSCK) {
            gpio_sck = pin;
        }
        if (GPIO_PIN_SPI_MOSI == UNDEF_PIN && pwm->val.mode == somMOSI) {
            gpio_mosi = pin;
        }
        if (GPIO_PIN_SPI_MISO == UNDEF_PIN && pwm->val.mode == somMISO) {
            gpio_miso = pin;
        }
        if (GPIO_PIN_SPI_NSS == UNDEF_PIN && pwm->val.mode == somNSS) {
            gpio_nss = pin;
        }
        if (GPIO_PIN_SPI_INT == UNDEF_PIN && pwm->val.mode == somINT) {
            gpio_int = pin;
        }
    }

    if(gpio_sck != UNDEF_PIN && gpio_miso != UNDEF_PIN && gpio_mosi != UNDEF_PIN && gpio_nss != UNDEF_PIN)
    {
        DBGLN("Starting SPI on SCK %d, MISO %d, MOSI %d" , gpio_sck, gpio_miso,gpio_mosi);
        _spi.begin(gpio_sck,gpio_miso,gpio_mosi,gpio_nss);

        spi_enabled = true;
    }
#endif    
}
void setupTargetCommon()
{
    setupWire();
	setupSPI();
}

void deferExecutionMicros(unsigned long us, std::function<void()> f)
{
    for (int i=0 ; i<maxDeferredFunctions ; i++)
    {
        if (deferred[i].function == nullptr)
        {
            deferred[i].started = micros();
            deferred[i].timeout = us;
            deferred[i].function = f;
            return;
        }
    }

    // Bail out, there are no slots available!
    DBGLN("No more deferred function slots available!");
}

void executeDeferredFunction(unsigned long now)
{
    // execute deferred function if its time has elapsed
    for (int i=0 ; i<maxDeferredFunctions ; i++)
    {
        if (deferred[i].function != nullptr && (now - deferred[i].started) > deferred[i].timeout)
        {
            deferred[i].function();
            deferred[i].function = nullptr;
        }
    }
}

/***
 * @brief Set a time in milliseconds to reboot the MCU from the main loop thread
 * */
void scheduleRebootTime(unsigned long inMs)
{
    rebootTime_Ms = millis() + inMs;
}

/**
 * @brief Call from the main thread to check if it is time to reboot. May not return.
 */
void checkRebootTime(unsigned long now)
{
    // If the reboot time is set and the current time is past the reboot time then reboot.
    // Wait for any pending config change to be committed first
    if (rebootTime_Ms != 0 && !config.IsModified() && now > rebootTime_Ms ) {
        ESP.restart();
    }
}