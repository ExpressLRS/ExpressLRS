#include <Arduino.h>
#include <stdint.h>

#define pin PA_7

void inline LEDsend_1(){
        digitalWriteFast(pin, HIGH);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); 
        digitalWriteFast(pin, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); 
}

void inline LEDsend_0(){
        digitalWriteFast(pin, HIGH);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); 
        digitalWriteFast(pin, LOW);
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); __NOP(); __NOP(); __NOP(); __NOP();
        __NOP(); 
}

uint8_t bitReverse(uint8_t input)
{
    uint8_t r = input; // r will be reversed bits of v; first get LSB of v
    uint8_t s = 8 - 1; // extra shift needed at end

    for (input >>= 1; input; input >>= 1)
    {
        r <<= 1;
        r |= input & 1;
        s--;
    }
    r <<= s; // shift when input's highest bits are zero
    return r;
}


void WS281BsetLED(uint8_t *Data) // takes RGB data
{
    pinMode(PA7, OUTPUT);

    uint8_t WS281BLedColourFlipped[3] = {0};

    WS281BLedColourFlipped[0] = bitReverse(Data[0]);
    WS281BLedColourFlipped[1] = bitReverse(Data[1]);
    WS281BLedColourFlipped[2] = bitReverse(Data[2]);

    uint32_t g = (WS281BLedColourFlipped[1]);
    uint32_t r = (WS281BLedColourFlipped[0]) << 8;
    uint32_t b = (WS281BLedColourFlipped[2]) << 16;

    uint32_t LedColourData = r + g + b;

    for (uint8_t i = 0; i < 24; i++)
    {
        ((LedColourData >> i) & 1) ? LEDsend_1() : LEDsend_0();
    }
    delayMicroseconds(50); // needed to latch in the values
}

void WS281BsetLED(uint8_t r, uint8_t g, uint8_t b) // takes RGB data
{
    uint8_t data[3] = {r, g, b};
    WS281BsetLED(data);
}

void WS281BsetLED(uint32_t color) // takes RGB data
{
    uint8_t data[3] = {0};
    data[0] = (color & 0x00FF0000) >> 16;
    data[1] = (color & 0x0000FF00) >> 8;
    data[2] = (color & 0x000000FF) >> 0;
    WS281BsetLED(data);
}