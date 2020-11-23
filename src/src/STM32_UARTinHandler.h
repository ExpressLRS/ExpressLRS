#include "CRSF.h"
#include "targets.h"

#if defined(TARGET_R9SLIMPLUS_RX)
#define CRSF_RX_SERIAL CrsfRxSerial
HardwareSerial CrsfRxSerial(USART3);
#else /* !TARGET_R9SLIMPLUS_RX */
#define CRSF_RX_SERIAL Serial
#endif /* TARGET_R9SLIMPLUS_RX */

extern CRSF crsf;

extern GENERIC_CRC8 crsf_crc;

uint8_t UARTinPacketPtr;
uint8_t UARTinPacketLen;
uint32_t UARTLastDataTime;
bool UARTframeActive = false;

uint8_t UARTinBuffer[256];

void STM32_RX_UARTprocessPacket()
{
    if (UARTinBuffer[2] == CRSF_FRAMETYPE_COMMAND)
    {
        Serial.println("Got CMD Packet");
        if (UARTinBuffer[3] == 0x62 && UARTinBuffer[4] == 0x6c)
        {
            delay(100);
            Serial.println("Jumping to Bootloader...");
            delay(100);
            HAL_NVIC_SystemReset();
        }
    }

    if (UARTinBuffer[2] == CRSF_FRAMETYPE_BATTERY_SENSOR)
    {
        crsf.TLMbattSensor.voltage = (UARTinBuffer[3] << 8) + UARTinBuffer[4];
        crsf.TLMbattSensor.current = (UARTinBuffer[5] << 8) + UARTinBuffer[6];
        crsf.TLMbattSensor.capacity = (UARTinBuffer[7] << 16) + (UARTinBuffer[8] << 8) + UARTinBuffer[9];
        crsf.TLMbattSensor.remaining = UARTinBuffer[10];
    }
}

void STM32_RX_HandleUARTin()
{
    while (CRSF_RX_SERIAL.available())
    {
        UARTLastDataTime = millis();
        char inChar = CRSF_RX_SERIAL.read();

        if ((inChar == CRSF_ADDRESS_CRSF_RECEIVER || inChar == CRSF_SYNC_BYTE) && UARTframeActive == false) // we got sync, reset write pointer
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
                while (CRSF_RX_SERIAL.available())
                {
                    (void)CRSF_RX_SERIAL.read();
                }
            }
        }

        if (UARTinPacketPtr == 64) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
        {
            UARTinPacketPtr = 0;
        }

        UARTinBuffer[UARTinPacketPtr] = inChar;
        UARTinPacketPtr++;

        if (UARTinPacketPtr == UARTinPacketLen + 2) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
        {
            char CalculatedCRC = crsf_crc.calc((uint8_t *)UARTinBuffer + 2, UARTinPacketPtr - 3);

            if (CalculatedCRC == inChar)
            {
                STM32_RX_UARTprocessPacket();

                UARTinPacketPtr = 0;
                UARTframeActive = false;
            }
            else
            {
                //Serial.println("UART CRC failure");
                //Serial.println(UARTinPacketPtr, HEX);
                //Serial.print("Expected: ");
                //Serial.println(CalculatedCRC, HEX);
                //Serial.print("Got: ");
                //Serial.println(inChar, HEX);
                //for (int i = 0; i < UARTinPacketPtr + 2; i++)
                // {
                //Serial.print(UARTinBuffer[i], HEX);
                //    Serial.print(" ");
                //}
                //Serial.println();
                //Serial.println();
                UARTframeActive = false;
                UARTinPacketPtr = 0;
                while (CRSF_RX_SERIAL.available())
                {
                    // dunno why but the flush() method wasn't working
                    // A: because flush() waits until all data is SENT aka TX buffer is empty
                    (void)CRSF_RX_SERIAL.read();
                }
            }
        }
    }
}
