#include "CRSF.h"

#include "../../src/utils.h"
#include "../../src/debug.h"

//U1RXD_IN_IDX
//U1TXD_OUT_IDX

void nullCallback(void){};

void (*CRSF::RCdataCallback1)() = &nullCallback; // function is called whenever there is new RC data.

void (*CRSF::disconnected)() = &nullCallback; // called when CRSF stream is lost
void (*CRSF::connected)() = &nullCallback;    // called when CRSF stream is regained

void (*CRSF::RecvParameterUpdate)() = &nullCallback; // called when recv parameter update req, ie from LUA

void CRSF::Begin()
{
    DEBUG_PRINTLN("CRSF UART LISTEN TASK STARTED");

    CRSFstate = false;
    GoodPktsCount = 0;
    BadPktsCount = 0;
    SerialInPacketStart = 0;
    SerialInPacketLen = 0;
    SerialInPacketPtr = 0;
    CRSFframeActive = false;

    _dev->flush_read();
}

uint8_t *CRSF::HandleUartIn(uint8_t inChar)
{
    uint8_t *packet_ptr = NULL;

    if (SerialInPacketPtr >= sizeof(SerialInBuffer))
    {
        // we reached the maximum allowable packet length,
        // so start again because shit fucked up hey.
        SerialInPacketPtr = 0;
        CRSFframeActive = false;
        BadPktsCount++;
    }

    // store byte
    SerialInBuffer[SerialInPacketPtr++] = inChar;

    if (CRSFframeActive == false)
    {
        if (inChar == CRSF_ADDRESS_CRSF_RECEIVER ||
            inChar == CRSF_ADDRESS_CRSF_TRANSMITTER ||
            inChar == CRSF_SYNC_BYTE)
        {
            CRSFframeActive = true;
            SerialInPacketLen = 0;
        }
        else
        {
            SerialInPacketPtr = 0;
        }
    }
    else
    {
        if (SerialInPacketLen == 0) // we read the packet length and save it
        {
            SerialInPacketLen = inChar;
            SerialInPacketStart = SerialInPacketPtr;
            if ((SerialInPacketLen < 2) || (CRSF_FRAME_SIZE_MAX < SerialInPacketLen))
            {
                // failure -> discard
                CRSFframeActive = false;
                SerialInPacketPtr = 0;
                BadPktsCount++;
            }
        }
        else
        {
            if ((SerialInPacketPtr - SerialInPacketStart) == (SerialInPacketLen))
            {
                uint8_t *payload = &SerialInBuffer[SerialInPacketStart];
                uint8_t CalculatedCRC = CalcCRC(payload, (SerialInPacketLen - 1));

                if (CalculatedCRC == inChar)
                {
                    packet_ptr = payload;
                    GoodPktsCount++;
                }
                else
                {
#if 0
                    DEBUG_PRINTLN("UART CRC failure");
                    for (int i = 0; i < (SerialInPacketLen + 2); i++)
                    {
                        DEBUG_PRINT(SerialInBuffer[i], HEX);
                        DEBUG_PRINT(" ");
                    }
                    DEBUG_PRINTLN();
#elif defined(DEBUG_SERIAL)
                    DEBUG_SERIAL.print('C');
                    DEBUG_SERIAL.write((uint8_t *)&SerialInBuffer[SerialInPacketStart - 2], SerialInPacketLen + 2);
#endif
                    BadPktsCount++;
                }

                // packet handled, start next
                CRSFframeActive = false;
                SerialInPacketPtr = 0;
                SerialInPacketLen = 0;
            }
        }
    }

    return packet_ptr;
}
