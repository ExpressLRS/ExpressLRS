#include "telemetry.h"
#include <cstring>

#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX) || defined(UNIT_TEST)

telemetry_state_s Telemetry::telemtry_state = TELEMTRY_IDLE;
char Telemetry::CRSFinBuffer[CRSF_MAX_PACKET_LEN + TELEMETRY_CRC_LENGTH] = {0};
char Telemetry::currentTelemetryByte = 0;
volatile crsf_telemetry_package_t* Telemetry::telemetryPackageHead = 0;

void Telemetry::AppendToPackage(volatile crsf_telemetry_package_t *current)
{
    current->data = new char[CRSFinBuffer[TELEMETRY_LENGTH_INDEX] + TELEMETRY_ADDITIONAL_LENGTH];
    memcpy(CRSFinBuffer, current->data, CRSFinBuffer[TELEMETRY_LENGTH_INDEX] + TELEMETRY_ADDITIONAL_LENGTH);
}

void Telemetry::AppendTelemetryPackage()
{
    volatile crsf_telemetry_package_t *current = telemetryPackageHead;
    volatile crsf_telemetry_package_t *tail = telemetryPackageHead;
    bool found = false;

    if (CRSFinBuffer[2] == CRSF_FRAMETYPE_COMMAND)
    {
        #if defined(PLATFORM_STM32)
            Serial.println("Got CMD Packet");
            if (CRSFinBuffer[3] == 0x62 && CRSFinBuffer[4] == 0x6c)
            {
                delay(100);
                Serial.println("Jumping to Bootloader...");
                delay(100);
                HAL_NVIC_SystemReset();
            }
        #endif
        return;
    }
    
    while (current != 0) {
        if (current->data[TELEMETRY_TYPE_INDEX] == CRSFinBuffer[TELEMETRY_TYPE_INDEX] && !current->locked)
        {
            delete [] current->data;
            AppendToPackage(current);
            found = true;
        }
        tail = current;
        current = current->next;
    }

    if (!found) 
    {
        if (telemetryPackageHead != 0) {
            tail->next = new crsf_telemetry_package_t;
            current = tail->next;
        }
        else 
        {
            telemetryPackageHead = new crsf_telemetry_package_t;
            current = telemetryPackageHead;
        }
        current->next = 0;
        AppendToPackage(current);
        current->locked = false;
    }
}

bool Telemetry::RXhandleUARTin(char data)
{
    switch(telemtry_state) {
        case TELEMTRY_IDLE:
            if (data == CRSF_ADDRESS_CRSF_RECEIVER || data == CRSF_SYNC_BYTE)
            {
                currentTelemetryByte = 0;
                telemtry_state = RECEIVING_LEGNTH;
                CRSFinBuffer[0] = data;
            }

            break;
        case RECEIVING_LEGNTH:
            if (data >= CRSF_MAX_PACKET_LEN)
            {
                telemtry_state = TELEMTRY_IDLE;
                return false;
            }
            else 
            {
                telemtry_state = RECEIVING_DATA;
                CRSFinBuffer[TELEMETRY_LENGTH_INDEX] = data;
            }

            break;
        case RECEIVING_DATA:
            CRSFinBuffer[currentTelemetryByte + TELEMETRY_ADDITIONAL_LENGTH] = data;

            if (CRSFinBuffer[TELEMETRY_LENGTH_INDEX] == currentTelemetryByte) 
            {
                // exclude first bytes (sync byte + length), skip last byte (submitted crc)
                /*if (data == CalcCRC((char *)CRSFinBuffer + TELEMETRY_ADDITIONAL_LENGTH, CRSFinBuffer[TELEMETRY_LENGTH_INDEX] - TELEMETRY_CRC_LENGTH))
                {
                    AppendTelemetryPackage();
                }*/
                telemtry_state = TELEMTRY_IDLE;
            }
            else
            {
                currentTelemetryByte++;
            }
            
            break;
    }

    return true;
}
#endif