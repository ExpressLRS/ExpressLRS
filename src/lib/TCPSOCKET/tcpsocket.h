#pragma once
#if defined(USE_MSP_WIFI)

#include <cstdint>
#include <cstring>
#include "ESPAsyncWebServer.h"
#include "FIFO.h"

#define BUFFER_OUTPUT_SIZE 1024
#define BUFFER_INPUT_SIZE 1024

// buffers reads and write to the specified TCP port

class TCPSOCKET
{
private:
    static TCPSOCKET *instance;
    //std::vector<AsyncClient *> clients; // a list to hold all clients for MSP to WIFI bridge nut not using multi-client for now

    AsyncServer *TCPserver;
    AsyncClient *TCPclient = NULL;
    uint32_t TCPport;
    const uint32_t clientTimeoutPeriod = 2000;
    uint32_t clientTimeoutLastData;

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleError(void *arg, AsyncClient *client, int8_t error);

    FIFO<BUFFER_OUTPUT_SIZE> FIFOout;
    FIFO<BUFFER_INPUT_SIZE> FIFOin;

public:
    TCPSOCKET(const uint32_t port);
    void begin();
    void handle();
    bool hasClient();
    uint16_t bytesReady(); // has x bytes in the input buffer ready
    bool write(uint8_t *data, uint16_t len);
    void read(uint8_t *data);
};

#endif