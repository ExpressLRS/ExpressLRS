#include "SerialTCP.h"
#include "logging.h"

#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)

SerialTCP::SerialTCP(Stream &output, Stream &input) : SerialIO(&output, &input)
{
    // Initialize TCP socket with default values - should be configured via initializeTCPSocket later
}

SerialTCP::~SerialTCP()
{
    // Cleanup if needed
}

void SerialTCP::initializeTCPSocket(ConnectionMode mode, uint16_t port, IPAddress remoteIP)
{
    tcpSocket.begin(mode, port, remoteIP);
    tcpInitialized = true;
}

void SerialTCP::sendQueuedData(uint32_t maxBytesToSend)
{
    if (!tcpInitialized)
    {
        return;
    }

    tcpSocket.handle();

    uint32_t bytesWritten = 0;
    while (tcpSocket.hasDataAvailable() && bytesWritten < maxBytesToSend)
    {
        size_t bytesRead = tcpSocket.read(tempBuffer, sizeof(tempBuffer));
        if (bytesRead == 0)
        {
            break;
        }

        if (_outputPort == nullptr)
        {
            break;
        }

        _outputPort->write(tempBuffer, bytesRead);
        _outputPort->flush();
        bytesWritten += bytesRead;
    }
}

void SerialTCP::processBytes(uint8_t *bytes, uint16_t size)
{
    if (tcpInitialized && size > 0)
    {
        tcpSocket.write(bytes, size);
        tcpSocket.handle();
    }
}

#endif
