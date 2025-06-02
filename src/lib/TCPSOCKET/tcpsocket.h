#pragma once
#if defined(USE_MSP_WIFI)

#include <cstdint>
#include <cstring>
#include "ESPAsyncWebServer.h"
#include "crsf2msp.h"
#include "msp2crsf.h"
#include "FIFO.h"

#define BUFFER_OUTPUT_SIZE 1024
#define BUFFER_INPUT_SIZE 1024

// buffers reads and write to the specified TCP port

class TCPSOCKET
{
private:
    static TCPSOCKET *instance;
    //std::vector<AsyncClient *> clients; // a list to hold all clients for MSP to WIFI bridge nut not using multi-client for now

    AsyncServer *TCPserver = nullptr;
    AsyncClient *TCPclient = nullptr;
    const uint32_t clientTimeoutPeriod = 2000;
    uint32_t clientTimeoutLastData = 0;

    static void handleNewClient(void *arg, AsyncClient *client);
    static void handleDataIn(void *arg, AsyncClient *client, void *data, size_t len);
    static void handleDisconnect(void *arg, AsyncClient *client);
    static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time);
    static void handleError(void *arg, AsyncClient *client, int8_t error);

    bool hasClient() const { return (TCPclient != nullptr); }
    void pumpData();
    void write(uint8_t *data, uint16_t len);
    void read(uint8_t *data);
    void clientConnect(AsyncClient *client);
    void clientDisconnect(AsyncClient *client);

    FIFO<BUFFER_OUTPUT_SIZE> *FIFOout = nullptr;
    FIFO<BUFFER_INPUT_SIZE> *FIFOin = nullptr;
    CROSSFIRE2MSP *crsf2msp = nullptr;

public:
    void begin();
    void handle();
    void crsfMspIn(uint8_t *data);
};

#endif