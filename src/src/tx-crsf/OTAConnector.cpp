#include "OTAConnector.h"

#include "CRSF.h"
#include "stubborn_sender.h"

extern StubbornSender MspSender;

OTAConnector::OTAConnector()
{
    // add the devices that we know are reachable via this connector
    addDevice(CRSF_ADDRESS_CRSF_RECEIVER);
    addDevice(CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void OTAConnector::forwardMessage(crsf_ext_header_t *message)
{
    const uint8_t length = message->frame_size + 2;
    AddMspMessage(length, (uint8_t *)message);
}

void OTAConnector::pumpMSPSender()
{
      static bool mspTransferActive = false;
    // sending is done and we need to update our flag
    if (mspTransferActive)
    {
        // unlock buffer for msp messages
        UnlockMspMessage();
        mspTransferActive = false;
    }
    // we are not sending so look for next msp package
    else
    {
        uint8_t* mspData;
        uint8_t mspLen;
        GetMspMessage(&mspData, &mspLen);
        // if we have a new msp package start sending
        if (mspData != nullptr)
        {
            MspSender.SetDataToTransmit(mspData, mspLen);
            mspTransferActive = true;
        }
    }
}

void OTAConnector::GetMspMessage(uint8_t **data, uint8_t *len)
{
    *len = MspDataLength;
    *data = (MspDataLength > 0) ? MspData : nullptr;
}

void OTAConnector::ResetMspQueue()
{
    MspWriteFIFO.flush();
    MspDataLength = 0;
    memset(MspData, 0, ELRS_MSP_BUFFER);
}

void OTAConnector::UnlockMspMessage()
{
    // the current msp message is sent so restore the next buffered write
    if (MspWriteFIFO.size() > 0)
    {
        MspWriteFIFO.lock();
        MspDataLength = MspWriteFIFO.pop();
        MspWriteFIFO.popBytes(MspData, MspDataLength);
        MspWriteFIFO.unlock();
    }
    else
    {
        // no msp message is ready to send currently
        MspDataLength = 0;
        memset(MspData, 0, ELRS_MSP_BUFFER);
    }
}

void OTAConnector::AddMspMessage(const uint8_t length, uint8_t* data)
{
    if (length > ELRS_MSP_BUFFER)
    {
        return;
    }

    // store next msp message
    if (MspDataLength == 0)
    {
        for (uint8_t i = 0; i < length; i++)
        {
            MspData[i] = data[i];
        }
        MspDataLength = length;
    }
    // store all write-requests since an update does send multiple writes
    else
    {
        MspWriteFIFO.lock();
        if (MspWriteFIFO.ensure(length + 1))
        {
            MspWriteFIFO.push(length);
            MspWriteFIFO.pushBytes((const uint8_t *)data, length);
        }
        MspWriteFIFO.unlock();
    }
}
