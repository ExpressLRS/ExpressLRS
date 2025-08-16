#include "NullHandset.h"
#include <options.h>

HardwareSerial Port(0);

void NullHandset::Begin()
{
    Port.begin(firmwareOptions.uart_baud);
}

void NullHandset::End()
{
}

bool NullHandset::IsArmed()
{
    return false;
}

void NullHandset::handleInput()
{
}

void NullHandset::write(uint8_t *data, uint8_t len)
{
    Port.write(data, len);
}

uint8_t NullHandset::readBytes(uint8_t *data, uint8_t len)
{
    return Port.readBytes(data, len);
}