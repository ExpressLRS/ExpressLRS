#ifndef UNIT_TEST

#include "RFAMP_hal.h"
#include "logging.h"

RFAMP_hal *RFAMP_hal::instance = NULL;

RFAMP_hal::RFAMP_hal()
{
    instance = this;
}

void RFAMP_hal::init()
{
    DBGLN("RFAMP_hal Init");

#if defined(PLATFORM_ESP32)
    #define SET_BIT(n) ((n != UNDEF_PIN) ? 1ULL << n : 0)

    txrx_disable_clr_bits = 0;
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    txrx_disable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);

    tx1_enable_set_bits = 0;
    tx1_enable_clr_bits = 0;
    tx1_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    tx1_enable_set_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    tx1_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    tx1_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);

    tx2_enable_set_bits = 0;
    tx2_enable_clr_bits = 0;
    tx2_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    tx2_enable_set_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);
    tx2_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    tx2_enable_clr_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);

    tx_all_enable_set_bits = 0;
    tx_all_enable_clr_bits = 0; 
    tx_all_enable_set_bits = tx1_enable_set_bits | tx2_enable_set_bits;
    tx_all_enable_clr_bits = tx1_enable_clr_bits | tx2_enable_clr_bits; 

    rx_enable_set_bits = 0;
    rx_enable_clr_bits = 0;
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_PA_ENABLE);
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_RX_ENABLE);
    rx_enable_set_bits |= SET_BIT(GPIO_PIN_RX_ENABLE_2);
    rx_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE);
    rx_enable_clr_bits |= SET_BIT(GPIO_PIN_TX_ENABLE_2);
#else
    rx_enabled = false;
    tx1_enabled = false;
    tx2_enabled = false;
#endif

    if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use PA enable pin: %d", GPIO_PIN_PA_ENABLE);
        pinMode(GPIO_PIN_PA_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
    }

    if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use TX pin: %d", GPIO_PIN_TX_ENABLE);
        pinMode(GPIO_PIN_TX_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
    }

    if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
    {
        DBGLN("Use RX pin: %d", GPIO_PIN_RX_ENABLE);
        pinMode(GPIO_PIN_RX_ENABLE, OUTPUT);
        digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
    }

    if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
    {
        DBGLN("Use TX_2 pin: %d", GPIO_PIN_TX_ENABLE_2);
        pinMode(GPIO_PIN_TX_ENABLE_2, OUTPUT);
        digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
    }

    if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
    {
        DBGLN("Use RX_2 pin: %d", GPIO_PIN_RX_ENABLE_2);
        pinMode(GPIO_PIN_RX_ENABLE_2, OUTPUT);
        digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
    }
}

void ICACHE_RAM_ATTR RFAMP_hal::TXenable(SX12XX_Radio_Number_t radioNumber)
{
#if defined(PLATFORM_ESP32)
    if (radioNumber == SX12XX_Radio_All)
    {
        GPIO.out_w1ts = tx_all_enable_set_bits;
        GPIO.out_w1tc = tx_all_enable_clr_bits;

        GPIO.out1_w1ts.data = tx_all_enable_set_bits >> 32;
        GPIO.out1_w1tc.data = tx_all_enable_clr_bits >> 32;
    }
    else if (radioNumber == SX12XX_Radio_2)
    {
        GPIO.out_w1ts = tx2_enable_set_bits;
        GPIO.out_w1tc = tx2_enable_clr_bits;

        GPIO.out1_w1ts.data = tx2_enable_set_bits >> 32;
        GPIO.out1_w1tc.data = tx2_enable_clr_bits >> 32;
    }
    else
    {
        GPIO.out_w1ts = tx1_enable_set_bits;
        GPIO.out_w1tc = tx1_enable_clr_bits;

        GPIO.out1_w1ts.data = tx1_enable_set_bits >> 32;
        GPIO.out1_w1tc.data = tx1_enable_clr_bits >> 32;
    }
#else
    if (!tx1_enabled && !tx2_enabled && !rx_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_PA_ENABLE, HIGH);
        }
    }
    if (rx_enabled)
    {
        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
        }
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
        }
        rx_enabled = false;
    }
    if (radioNumber == SX12XX_Radio_1 && !tx1_enabled)
    {
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE, HIGH);
        }
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
        }
        tx1_enabled = true;
        tx2_enabled = false;
    }
    if (radioNumber == SX12XX_Radio_2 && !tx2_enabled)
    {
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
        }
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE_2, HIGH);
        }
        tx1_enabled = false;
        tx2_enabled = true;
    }
#endif
}

void ICACHE_RAM_ATTR RFAMP_hal::RXenable()
{
#if defined(PLATFORM_ESP32)
    GPIO.out_w1ts = rx_enable_set_bits;
    GPIO.out_w1tc = rx_enable_clr_bits;

    GPIO.out1_w1ts.data = rx_enable_set_bits >> 32;
    GPIO.out1_w1tc.data = rx_enable_clr_bits >> 32;
#else
    if (!rx_enabled)
    {
        if (!tx1_enabled && !tx2_enabled && GPIO_PIN_PA_ENABLE != UNDEF_PIN)
            digitalWrite(GPIO_PIN_PA_ENABLE, HIGH);

        if (tx1_enabled && GPIO_PIN_TX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
            tx1_enabled = false;
        }

        if (tx2_enabled && GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
            tx2_enabled = false;
        }

        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE, HIGH);
        }
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE_2, HIGH);
        }

        rx_enabled = true;
    }
#endif
}

void ICACHE_RAM_ATTR RFAMP_hal::TXRXdisable()
{
#if defined(PLATFORM_ESP32)
    GPIO.out_w1tc = txrx_disable_clr_bits;
    GPIO.out1_w1tc.data = txrx_disable_clr_bits >> 32;
#else
    if (rx_enabled)
    {
        if (GPIO_PIN_RX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE, LOW);
        }
        if (GPIO_PIN_RX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_RX_ENABLE_2, LOW);
        }
        rx_enabled = false;
    }
    if (tx1_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
        }
        if (GPIO_PIN_TX_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE, LOW);
        }
        tx1_enabled = false;
    }
    if (tx2_enabled)
    {
        if (GPIO_PIN_PA_ENABLE != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_PA_ENABLE, LOW);
        }
        if (GPIO_PIN_TX_ENABLE_2 != UNDEF_PIN)
        {
            digitalWrite(GPIO_PIN_TX_ENABLE_2, LOW);
        }
        tx2_enabled = false;
    }
#endif
}

#endif // UNIT_TEST
