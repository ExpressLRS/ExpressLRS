#include <cstdint>
#include <cstring>
#include <telemetry.h>
#include <crsf_protocol.h>

#if defined(UNIT_TEST)
#include <iostream>
using namespace std;
#endif
#define TARGET_R9M_RX

#if defined(PLATFORM_ESP8266) || defined(TARGET_R9M_RX) || defined(UNIT_TEST)

telemetry_state_s Telemetry::telemtry_state = TELEMTRY_IDLE;
uint8_t Telemetry::CRSFinBuffer[CRSF_MAX_PACKET_LEN] = {0};
uint8_t Telemetry::currentTelemetryByte = 0;
volatile crsf_telemetry_package_t* Telemetry::telemetryPackageHead = 0;
bool Telemetry::callBootloader = false;

volatile crsf_telemetry_package_t* Telemetry::GetPackageHead()
{
    telemetryPackageHead->locked = true;
    return telemetryPackageHead;
}

uint8_t Telemetry::QueueLength()
{
    uint8_t count = 0;
    volatile crsf_telemetry_package_t *current = telemetryPackageHead;

    while (current != 0)
    {
        count++;
        current = current->next;
    }

    return count;
}

void Telemetry::DeleteHead()
{
    volatile crsf_telemetry_package_t *next = telemetryPackageHead->next;
    delete [] telemetryPackageHead->data;
    delete telemetryPackageHead;
    telemetryPackageHead = next;
}

void Telemetry::AppendToPackage(volatile crsf_telemetry_package_t *current)
{
    current->data = new uint8_t[CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] + CRSF_FRAME_NOT_COUNTED_BYTES];
    memcpy(current->data, CRSFinBuffer, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] + CRSF_FRAME_NOT_COUNTED_BYTES);
}

void Telemetry::AppendTelemetryPackage()
{
    volatile crsf_telemetry_package_t *current = telemetryPackageHead;
    volatile crsf_telemetry_package_t *tail = telemetryPackageHead;
    bool found = false;

    if (CRSFinBuffer[2] == CRSF_FRAMETYPE_COMMAND && CRSFinBuffer[3] == 0x62 && CRSFinBuffer[4] == 0x6c)
    {
        callBootloader = true;
        return;
    }
    
    while (current != 0) {
        if (current->data[CRSF_TELEMETRY_TYPE_INDEX] == CRSFinBuffer[CRSF_TELEMETRY_TYPE_INDEX])
        {
            if (!current->locked)
            {
                delete [] current->data;
                AppendToPackage(current);
            }
            
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

void Telemetry::ResetState()
{
    telemtry_state = TELEMTRY_IDLE;
    currentTelemetryByte = 0;

    while (telemetryPackageHead != 0)  
    {  
        DeleteHead();
    }
}

bool Telemetry::RXhandleUARTin(uint8_t data)
{
    switch(telemtry_state) {
        case TELEMTRY_IDLE:
            if (data == CRSF_ADDRESS_CRSF_RECEIVER || data == CRSF_SYNC_BYTE)
            {
                currentTelemetryByte = 0;
                telemtry_state = RECEIVING_LEGNTH;
                CRSFinBuffer[0] = data;
            }
            else {
                return false;
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
                CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] = data;
            }

            break;
        case RECEIVING_DATA:
            CRSFinBuffer[currentTelemetryByte + CRSF_FRAME_NOT_COUNTED_BYTES] = data;
            currentTelemetryByte++;
            if (CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] == currentTelemetryByte) 
            {
                // exclude first bytes (sync byte + length), skip last byte (submitted crc)
                uint8_t crc = CalcCRC(CRSFinBuffer + CRSF_FRAME_NOT_COUNTED_BYTES, CRSFinBuffer[CRSF_TELEMETRY_LENGTH_INDEX] - CRSF_TELEMETRY_CRC_LENGTH);
                telemtry_state = TELEMTRY_IDLE;

                if (data == crc)
                {
                    AppendTelemetryPackage();
                    return true;
                }
                #if defined(UNIT_TEST)
                if (data != crc) 
                {
                    cout << "invalid " << (int)crc  << '\n';   
                }
                #endif
                
                return false;
            }
            
            break;
    }

    return true;
}
#endif