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
        Serial.println("Got CMD Packet");
        if (UARTinBuffer[3] == 0x62 && UARTinBuffer[4] == 0x6c)
        {
            delay(100);
            Serial.println("Jumping to Bootloader...");
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
                Serial.println("CRC failure");
                Serial.println(UARTinPacketPtr, HEX);
                Serial.print("Expected: ");
                Serial.println(CalculatedCRC, HEX);
                Serial.print("Got: ");
                Serial.println(inChar, HEX);
                for (int i = 0; i < UARTinPacketPtr + 2; i++)
                {
                    Serial.print(UARTinBuffer[i], HEX);
                    Serial.print(" ");
                }
                Serial.println();
                Serial.println();
                UARTframeActive = false;
                UARTinPacketPtr = 0;
                Serial.flush();
            }
        }
    }
}

//
//             SerialInPacketPtr = 0;
//             CRSFframeActive = false;
//             //gpio_set_drive_capability((gpio_num_t)CSFR_TXpin_Module, GPIO_DRIVE_CAP_2);
//             //taskYIELD();
//             vTaskDelay(xDelay1);

//             uint8_t peekVal = SerialOutFIFO.peek(); // check if we have data in the output FIFO that needs to be written
//             if (peekVal > 0)
//             {
//                 if (SerialOutFIFO.size() >= peekVal)
//                 {
//                     CRSF::duplex_set_TX();

//                     uint8_t OutPktLen = SerialOutFIFO.pop();
//                     uint8_t OutData[OutPktLen];

//                     SerialOutFIFO.popBytes(OutData, OutPktLen);
//                     CRSF::Port.write(OutData, OutPktLen); // write the packet out

//                     vTaskDelay(xDelay1);
//                     CRSF::duplex_set_RX();
//                     FlushSerial(); // we don't need to read back the data we just wrote
//                 }
//             }
//             vTaskDelay(xDelay1);
//             //gpio_set_drive_capability((gpio_num_t)CSFR_TXpin_Module, GPIO_DRIVE_CAP_0);
//         }
//         else
//         {
//             Serial.println("CRC failure");
//             Serial.println(SerialInPacketPtr, HEX);
//             Serial.print("Expected: ");
//             Serial.println(CalculatedCRC, HEX);
//             Serial.print("Got: ");
//             Serial.println(inChar, HEX);
//             for (int i = 0; i < SerialInPacketLen + 2; i++)
//             {
//                 Serial.print(SerialInBuffer[i], HEX);
//                 Serial.print(" ");
//             }
//             Serial.println();
//             Serial.println();
//             CRSFframeActive = false;
//             SerialInPacketPtr = 0;
//             FlushSerial();
//             vTaskDelay(xDelay1);
//         }
//     }
// }
//}
