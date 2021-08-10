#include <BAUD_DETECT.h>
#include "targets.h"

BAUD_DETECT *BAUD_DETECT::instance = NULL;

#ifdef PLATFORM_ESP32
static hw_timer_t *timer = NULL;

#elif PLATFORM_ESP8266

#elif PLATFORM_STM32

#if defined(TIM1)
HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM1);
#else
// FM30_mini (STM32F373xC) no advanced timer but TIM2 is 32-bit general purpose
HardwareTimer(*hwTimer::MyTim) = new HardwareTimer(TIM2);
#endif

#endif

BAUD_DETECT::BAUD_DETECT()
{
    instance = this;
}

void BAUD_DETECT::begin(uint8_t pin)
{
    Serial.println("UART BAUDRATE DETECT");
    uartGPIO = pin;
    measIntervalsCount = 0;
    measDone = false;
    calcDone = false;

#ifdef PLATFORM_ESP32
    timer = timerBegin(0, 1, true); // 80MHz ticks
    //timerSetAutoReload(timer, true);
    //timerSetCountUp(timer, true);

#elif PLATFORM_STM32
    MyTim->setMode(1, TIMER_DISABLED);
    MyTim->setPrescaleFactor(1);
    MyTim->setCount(0);
    MyTim->resume();
    MyTim->refresh();

#endif

    attachInterrupt(digitalPinToInterrupt(pin), instance->isr, CHANGE);
}

void ICACHE_RAM_ATTR BAUD_DETECT::isr()
{
#ifdef PLATFORM_ESP32

    uint32_t result = timerRead(timer);

#elif PLATFORM_STM32

    uint32_t result = getCount();

#endif

    if (instance->measIntervalsCount < 1023)
    {
        instance->measIntervalsCount++;
        instance->measIntervals[instance->measIntervalsCount] = result - instance->measIntervals[instance->measIntervalsCount-1];
    }
    else
    {
        instance->measIntervalsCount = 0;
        detachInterrupt(digitalPinToInterrupt(instance->uartGPIO));
        instance->measDone = true;
        instance->calc();
    }
}

void ICACHE_RAM_ATTR BAUD_DETECT::calc()
{
    for (int i = 0; i < 1023; i++)
    { ///insert and sort the values
        int value = measIntervals[i];
        int j;
        if (value < sortedValues[0] || i == 0)
        {
            j = 0; //insert at first position
        }
        else
        {
            for (j = 1; j < i; j++)
            {
                if (sortedValues[j - 1] <= value && sortedValues[j] >= value)
                {
                    // j is insert position
                    break;
                }
            }
        }
        for (int k = i; k > j; k--)
        {
            // move all values higher than current reading up one position
            sortedValues[k] = sortedValues[k - 1];
        }
        sortedValues[j] = value; //insert current reading
    }
    calcDone = true;
}

void BAUD_DETECT::print()
{
    for (int i = 0; i < 30; i++)
    {
        Serial.println(sortedValues[i]);
    }

    for (int i = 0; i < 1023; i++)
    {
        sortedValues[i] = 0;
    }
}

bool BAUD_DETECT::status()
{
    return measDone && calcDone;
};

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"

uint32_t BAUD_DETECT::uart_baud_detect2(uart_port_t uart_num)
{
    ESP_ERROR_CHECK(gpio_set_direction((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, GPIO_MODE_INPUT));
    #ifdef UART_INVERTED
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, true);
    gpio_pulldown_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    gpio_pullup_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    #else
    gpio_matrix_in((gpio_num_t)GPIO_PIN_RCSIGNAL_RX, U1RXD_IN_IDX, false);
    gpio_pullup_en((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    gpio_pulldown_dis((gpio_num_t)GPIO_PIN_RCSIGNAL_RX);
    #endif

	
    int low_period = 0;
    int high_period = 0;
    uint32_t intena_reg = READ_PERI_REG(UART_INT_ENA_REG(uart_num)); //UART[uart_num]->int_ena.val;
    //Disable the interruput.
    WRITE_PERI_REG(UART_INT_RAW_REG(uart_num), 0);//->int_ena.val = 0;
    WRITE_PERI_REG(UART_INT_CLR_REG(uart_num), 0);//UART[uart_num]->int_clr.val = ~0;
    //Filter
    SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), 4);//UART[uart_num]->auto_baud.glitch_filt = 4;
	
    //Clear the previous result
    SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), (UART_GLITCH_FILT << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN));
    SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), 0x08 << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN);
    delay(1000);
    // while(GET_PERI_REG_MASK(UART_RXD_CNT_REG(uart_num), UART_RXD_EDGE_CNT) < 100) {
       
    // };
    low_period = GET_PERI_REG_MASK(UART_LOWPULSE_REG(uart_num), UART_LOWPULSE_MIN_CNT);//UART[uart_num]->lowpulse.min_cnt;
    high_period = GET_PERI_REG_MASK(UART_HIGHPULSE_REG(uart_num), UART_HIGHPULSE_MIN_CNT);//UART[uart_num]->highpulse.min_cnt;
    // disable the baudrate detection
    SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), (UART_GLITCH_FILT << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN)); //UART[uart_num]->auto_baud.en = 0;
    //Reset the fifo;
    //uart_reset_rx_fifo(uart_num);
    WRITE_PERI_REG(UART_INT_ENA_REG(uart_num), intena_reg);//UART[uart_num]->int_ena.val = intena_reg;
    ///Set the clock divider reg
    ///UART[uart_num]->clk_div.div_int = (low_period > high_period) ? high_period : low_period;

    ///Return the divider. baud = APB / divider;
    //return (low_period > high_period) ? high_period : low_period;
	int32_t divisor = (low_period > high_period) ? high_period : low_period;
	int32_t baudrate = UART_CLK_FREQ / divisor;

    static const int default_rates[] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 256000, 460800, 921600, 1843200, 3686400};

    size_t i;
    for (i = 1; i < sizeof(default_rates) / sizeof(default_rates[0]) - 1; i++)	// find the nearest real baudrate
    {
        if (baudrate <= default_rates[i])
        {
            if (baudrate - default_rates[i - 1] < default_rates[i] - baudrate) {
                i--;
            }
            break;
        }
    }

    return default_rates[i];

    
    // int low_period = 0;
    // int high_period = 0;
    // uint32_t intena_reg = UART[uart_num]->int_ena.val;
    // //Disable the interruput.
    // UART[uart_num]->int_ena.val = 0;
    // UART[uart_num]->int_clr.val = ~0;
    // //Filter
    // UART[uart_num]->auto_baud.glitch_filt = 4;
    // //Clear the previous result
    // UART[uart_num]->auto_baud.en = 0;
    // UART[uart_num]->auto_baud.en = 1;
    // while(UART[uart_num]->rxd_cnt.edge_cnt < 100) {
    //     ets_delay_us(10);
    //      Serial.println("x");
    // }
    // low_period = UART[uart_num]->lowpulse.min_cnt;
    // high_period = UART[uart_num]->highpulse.min_cnt;
    // // disable the baudrate detection
    // UART[uart_num]->auto_baud.en = 0;
    // //Reset the fifo;
    // uart_reset_rx_fifo(uart_num);
    // UART[uart_num]->int_ena.val = intena_reg;
    // //Set the clock divider reg
    // //UART[uart_num]->clk_div.div_int = (low_period > high_period) ? high_period : low_period;

    // //Return the divider. baud = APB / divider;
    // return (low_period > high_period) ? high_period : low_period;;

    // Serial.println("1");
	
    // int low_period = 0;
    // int high_period = 0;
    // uint32_t intena_reg = READ_PERI_REG(UART_INT_ENA_REG(uart_num)); //UART[uart_num]->int_ena.val;
    // //Disable the interruput.
    // WRITE_PERI_REG(UART_INT_RAW_REG(uart_num), 0);//->int_ena.val = 0;
    // WRITE_PERI_REG(UART_INT_CLR_REG(uart_num), 0);//UART[uart_num]->int_clr.val = ~0;
    // //Filter
    // SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), 4);//UART[uart_num]->auto_baud.glitch_filt = 4;

    // Serial.println("2");
	
    // //Clear the previous result
    // SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), (UART_GLITCH_FILT << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN));
    // SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), 0x08 << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN);
    // while(GET_PERI_REG_MASK(UART_RXD_CNT_REG(uart_num), UART_RXD_EDGE_CNT) < 250) {
    //    //vTaskDelay(1);
    //    delay(100);
    //    Serial.println("x");
    // }

    // Serial.println("3");

    // low_period = GET_PERI_REG_MASK(UART_LOWPULSE_REG(uart_num), UART_LOWPULSE_MIN_CNT);//UART[uart_num]->lowpulse.min_cnt;
    // high_period = GET_PERI_REG_MASK(UART_HIGHPULSE_REG(uart_num), UART_HIGHPULSE_MIN_CNT);//UART[uart_num]->highpulse.min_cnt;
    // // disable the baudrate detection
    // SET_PERI_REG_MASK(UART_AUTOBAUD_REG(uart_num), (UART_GLITCH_FILT << UART_GLITCH_FILT_S | UART_AUTOBAUD_EN)); //UART[uart_num]->auto_baud.en = 0;
    // //Reset the fifo;
    // //uart_reset_rx_fifo(uart_num);
    // WRITE_PERI_REG(UART_INT_ENA_REG(uart_num), intena_reg);//UART[uart_num]->int_ena.val = intena_reg;
    // ///Set the clock divider reg
    // ///UART[uart_num]->clk_div.div_int = (low_period > high_period) ? high_period : low_period;

    // ///Return the divider. baud = APB / divider;
    // //return (low_period > high_period) ? high_period : low_period;
	// int32_t divisor = (low_period > high_period) ? high_period : low_period;
	// int32_t baudrate = UART_CLK_FREQ / divisor;

    // static const int default_rates[] = {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 74880, 115200, 230400, 256000, 460800, 921600, 1843200, 3686400};

    // size_t i;
    // for (i = 1; i < sizeof(default_rates) / sizeof(default_rates[0]) - 1; i++)	// find the nearest real baudrate
    // {
    //     if (baudrate <= default_rates[i])
    //     {
    //         if (baudrate - default_rates[i - 1] < default_rates[i] - baudrate) {
    //             i--;
    //         }
    //         break;
    //     }
    // }

    // return default_rates[i];
}