#pragma once

#include <stdint.h>

class Channels;
class TransportLayer;

class RXModule
{
public:
  virtual void begin(TransportLayer* dev);

  virtual void sendRCFrameToFC(Channels* chan) {}
  virtual void sendLinkStatisticsToFC(Channels* chan) {}
  virtual void sendMSPFrameToFC(uint8_t* data) {}

protected:
  TransportLayer* _dev = nullptr;
};
