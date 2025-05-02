#include "TXOTAConnector.h"

#include "common.h"
#include "stubborn_sender.h"

extern StubbornSender MspSender;

TXOTAConnector::TXOTAConnector()
{
    // add the devices that we know are reachable via this connector
    addDevice(CRSF_ADDRESS_CRSF_RECEIVER);
    addDevice(CRSF_ADDRESS_FLIGHT_CONTROLLER);
}

void TXOTAConnector::pumpSender()
{
    static bool transferActive = false;
    // sending is done and we need to update our flag
    if (transferActive)
    {
        // unlock buffer for msp messages
        unlockMessage();
        transferActive = false;
    }
    // we are not sending so look for next msp package
    if (!transferActive)
    {
        // if we have a new msp package start sending
        if (currentTransmissionLength > 0)
        {
            MspSender.SetDataToTransmit(currentTransmissionBuffer, currentTransmissionLength);
            transferActive = true;
        }
    }
}

void TXOTAConnector::resetOutputQueue()
{
    outputQueue.flush();
    currentTransmissionLength = 0;
}

void TXOTAConnector::unlockMessage()
{
    // the current msp message is sent so restore the next buffered write
    if (outputQueue.size() > 0)
    {
        outputQueue.lock();
        currentTransmissionLength = outputQueue.pop();
        outputQueue.popBytes(currentTransmissionBuffer, currentTransmissionLength);
        outputQueue.unlock();
    }
    else
    {
        // no msp message is ready to send currently
        currentTransmissionLength = 0;
    }
}

void TXOTAConnector::forwardMessage(const crsf_header_t *message)
{
    if (connectionState == connected)
    {
        const uint8_t length = message->frame_size + 2;
        if (length > ELRS_MSP_BUFFER)
        {
            return;
        }

        // store next msp message
        const auto data = (uint8_t *)message;
        if (currentTransmissionLength == 0)
        {
            for (uint8_t i = 0; i < length; i++)
            {
                currentTransmissionBuffer[i] = data[i];
            }
            currentTransmissionLength = length;
        }
        // store all write-requests since an update does send multiple writes
        else
        {
            outputQueue.lock();
            if (outputQueue.ensure(length + 1))
            {
                outputQueue.push(length);
                outputQueue.pushBytes((const uint8_t *)data, length);
            }
            outputQueue.unlock();
        }
    }
}
