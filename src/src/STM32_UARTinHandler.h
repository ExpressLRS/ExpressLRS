#include "CRSF.h"
#include "targets.h"

uint8_t UARTinPacketPtr;
uint8_t UARTinPacketLen;
uint32_t UARTLastDataTime;
bool UARTframeActive = false;

uint8_t UARTinBuffer[256];

void STM32_RX_UARTprocessPacket()
{
    if (UARTinBuffer[2] == CRSF_FRAMETYPE_COMMAND)
    {
        DEBUG_PRINTLN("Got CMD Packet");
        if (UARTinBuffer[3] == 0x62 && UARTinBuffer[4] == 0x6c)
        {
            delay(100);
            DEBUG_PRINTLN("Jumping to Bootloader...");
            delay(100);
            HAL_NVIC_SystemReset();
        }
    }
}

void STM32_RX_HandleUARTin()
{
    while (Serial.available())
    {
        UARTLastDataTime = millis();
        char inChar = Serial.read();

        if ((inChar == CRSF_ADDRESS_CRSF_RECEIVER) && UARTframeActive == false) // we got sync, reset write pointer
        {
            UARTinPacketPtr = 0;
            UARTframeActive = true;
        }

        if (UARTinPacketPtr == 1 && UARTframeActive == true) // we read the packet length and save it
        {
            UARTinPacketLen = inChar;
            UARTframeActive = true;
        }

        if (UARTframeActive && UARTinPacketPtr > 0) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
        {
            if (UARTinPacketPtr > UARTinPacketLen + 2)
            {
                UARTinPacketPtr = 0;
                UARTframeActive = false;
                Serial.flush();
            }
        }

        UARTinBuffer[UARTinPacketPtr] = inChar;
        UARTinPacketPtr++;

        if (UARTinPacketPtr == UARTinPacketLen + 2) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
        {
            char CalculatedCRC = CalcCRC((uint8_t *)UARTinBuffer + 2, UARTinPacketPtr - 3);

            if (CalculatedCRC == inChar)
            {
                STM32_RX_UARTprocessPacket();

                UARTinPacketPtr = 0;
                UARTframeActive = false;
            }
            else
            {
                //DEBUG_PRINTLN("UART CRC failure");
                //DEBUG_PRINTLN(UARTinPacketPtr, HEX);
                //DEBUG_PRINT("Expected: ");
                //DEBUG_PRINTLN(CalculatedCRC, HEX);
                //DEBUG_PRINT("Got: ");
                //DEBUG_PRINTLN(inChar, HEX);
                //for (int i = 0; i < UARTinPacketPtr + 2; i++)
                // {
                //DEBUG_PRINT(UARTinBuffer[i], HEX);
                //    DEBUG_PRINT(" ");
                //}
                //DEBUG_PRINTLN();
                //DEBUG_PRINTLN();
                UARTframeActive = false;
                UARTinPacketPtr = 0;
                Serial.flush();
            }
        }
    }
}
