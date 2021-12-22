#include "rx_crsf.h"
#include "logging.h"

// uint32_t CROSSFIRE::CRSFbaudrates[] = {400000, 115200, 5250000, 3750000, 1870000, 921600};

CROSSFIRE::CROSSFIRE(HardwareSerial *obj)
{
    this->UARTport = obj;
}

void CROSSFIRE::begin()
{
    UARTinit();
}

void CROSSFIRE::end()
{
    uint32_t startTime = millis();
    while (UARTinFIFO.peek() > 0)
    {
        UARTout(); // empty the buffer
        if (millis() - startTime > 1000)
        {
            break;
        }
    }
    DBGLN("CRSF UART stopped");
}

void CROSSFIRE::UARTflush()
{
    while (UARTport->available())
    {
        UARTport->read();
    }
}

void CROSSFIRE::UARTinit()
{
    DBGLN("Starting CRSF");
    UARTbufWriteIdx = 0;
    UARTbufLen = 0;
    UARTgoodPktsCount = 0;
    UARTbadPktsCount = 0;
    UARTwatchdogLastReset = millis();

    portDISABLE_INTERRUPTS();
    this->UARTport->begin(CRSFbaudrates[UARTcurrentBaudIdx], SERIAL_8N1,
                          GPIO_PIN_RCSIGNAL_RX, GPIO_PIN_RCSIGNAL_TX,
                          false, 500);

    UARTduplexSetRX();
    portENABLE_INTERRUPTS();
    UARTflush();
