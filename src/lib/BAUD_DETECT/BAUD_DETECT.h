#include <Arduino.h>

// #define BAUD_TO_US(baud) (1000000L / baud)
// #define PERIOD_US_TO_BAUD(period) (1000000L / period)

// typedef enum
// {
//     BAUD_TO_US(115200),
//     BAUD_TO_US(400000)

// } BAUDDECT_EXPECTED_BAUDRATES_IN_US_t;

#include "driver/uart.h"

class BAUD_DETECT
{
private:
    uint32_t measIntervals[1024] = {0};
    uint32_t sortedValues[1024] = {0};
    uint32_t measIntervalsCount = 0;
    bool calcDone = false;
    bool measDone = false;
    uint8_t uartGPIO = -1;
    /* data */
public:
    BAUD_DETECT();
    static BAUD_DETECT *instance;
    void begin(uint8_t pin);
    void calc();
    bool status();
    static void isr();
    void print();
    uint32_t uart_baud_detect2(uart_port_t uart_num);
};



