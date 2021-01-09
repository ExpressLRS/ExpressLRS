#include "CRSF.h"
#include "targets.h"

//#define DEBUG_RX_UART_IN

#if defined(TARGET_RX_GHOST_ATTO_V1) /* !TARGET_RX_GHOST_ATTO_V1 */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART1, HALF_DUPLEX_ENABLED);
#elif defined(TARGET_R9SLIMPLUS_RX) /* !TARGET_R9SLIMPLUS_RX */
    #define CRSF_RX_SERIAL CrsfRxSerial
    HardwareSerial CrsfRxSerial(USART3);
#else
    #define CRSF_RX_SERIAL Serial
#endif

extern CRSF crsf;
extern GENERIC_CRC8 crsf_crc;

uint8_t UARTinPacketPtr;
uint8_t UARTinPacketLen;
uint32_t UARTinLastDataTime;
uint32_t UARTinLastPacketTime;
bool UARTframeActive = false;

uint8_t UARTinBuffer[256];

void RX_UARTinProcessPacket()
{
    if (UARTinBuffer[2] == CRSF_FRAMETYPE_COMMAND)
    {
#ifdef PLATFORM_STM32
        #ifdef DEBUG_RX_UART_IN
        Serial.println("Got CMD Packet");
        #endif
        if (UARTinBuffer[3] == 0x62 && UARTinBuffer[4] == 0x6c)
        {
            delay(100);
            Serial.println("Jumping to Bootloader...");
            delay(100);
            HAL_NVIC_SystemReset();
        }
#endif
    }

    switch (UARTinBuffer[2])
    {
    case CRSF_FRAMETYPE_BATTERY_SENSOR:
        crsf.TLMbattSensor.voltage = (UARTinBuffer[3] << 8) + UARTinBuffer[4];
        crsf.TLMbattSensor.current = (UARTinBuffer[5] << 8) + UARTinBuffer[6];
        crsf.TLMbattSensor.capacity = (UARTinBuffer[7] << 16) + (UARTinBuffer[8] << 8) + UARTinBuffer[9];
        crsf.TLMbattSensor.remaining = UARTinBuffer[9];
        break;
        
    case CRSF_FRAMETYPE_GPS:
        crsf.TLMGPSsensor.latitude = (UARTinBuffer[3] << 24) + (UARTinBuffer[4] << 16) + (UARTinBuffer[5] << 8) + (UARTinBuffer[6] << 0);
        crsf.TLMGPSsensor.longitude = (UARTinBuffer[7] << 24) + (UARTinBuffer[8] << 16) + (UARTinBuffer[9] << 8) + (UARTinBuffer[10] << 0);
        crsf.TLMGPSsensor.speed = (UARTinBuffer[11] << 8) + (UARTinBuffer[12] << 0);
        crsf.TLMGPSsensor.headng = (UARTinBuffer[13] << 8) + (UARTinBuffer[14] << 0);
        crsf.TLMGPSsensor.alt = (UARTinBuffer[15] << 8) + (UARTinBuffer[16] << 0);
        crsf.TLMGPSsensor.sats = (UARTinBuffer[17]);
        break;

    default:
        break;
    }
}

void RX_UARTinHandle()
{
    while (Serial.available())
    {
        UARTinLastDataTime = millis();
        char inChar = Serial.read();

        if (UARTframeActive == false)
        {
            // stage 1 wait for sync byte //
            if (inChar == CRSF_ADDRESS_CRSF_TRANSMITTER || inChar == CRSF_SYNC_BYTE) // we got sync, reset write pointer
            {
                UARTinPacketPtr = 0;
                UARTinPacketLen = 0;
                UARTframeActive = true;
                UARTinBuffer[UARTinPacketPtr] = inChar;
                UARTinPacketPtr++;
            }
        }
        else // frame is active so we do the processing
        {
            // first if things have gone wrong //
            if (UARTinPacketPtr > CRSF_MAX_PACKET_LEN - 1) // we reached the maximum allowable packet length, so start again because shit fucked up hey.
            {
                UARTinPacketPtr = 0;
                UARTinPacketLen = 0;
                UARTframeActive = false;
                return;
            }
            // special case where we save the expected pkt len to buffer //
            if (UARTinPacketPtr == 1)
            {
                if (inChar <= CRSF_MAX_PACKET_LEN)
                {
                    UARTinPacketLen = inChar;
                }
                else
                {
                    UARTinPacketPtr = 0;
                    UARTinPacketLen = 0;
                    UARTframeActive = false;
                    return;
                }
            }

            UARTinBuffer[UARTinPacketPtr] = inChar;
            UARTinPacketPtr++;

            if (UARTinPacketPtr == UARTinPacketLen + 2) // plus 2 because the packlen is referenced from the start of the 'type' flag, IE there are an extra 2 bytes.
            {
                uint8_t CalculatedCRC = crsf_crc.calc((uint8_t *)UARTinBuffer + 2, UARTinPacketPtr - 3);

                if (CalculatedCRC == inChar)
                {
                    UARTinLastPacketTime = millis();
                    RX_UARTinProcessPacket();
                    UARTinPacketPtr = 0;
                    UARTinPacketLen = 0;
                    UARTframeActive = false;
                }
                else
                {
                    UARTinPacketPtr = 0;
                    UARTinPacketLen = 0;
                    UARTframeActive = false;
                    #ifdef DEBUG_RX_UART_IN
                    Serial.println("UART in CRC failure");
                    #endif
                    while (Serial.available())
                    {
                        Serial.read(); // empty the read buffer
                    }
                }
            }
        }
    }
}