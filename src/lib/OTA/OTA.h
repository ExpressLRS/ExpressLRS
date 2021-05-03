#ifndef H_OTA
#define H_OTA

#include <stdint.h>

struct Channels;

// expresslrs packet header types
// 00 -> standard 4 channel data packet
// 01 -> switch data packet
// 11 -> tlm packet
// 10 -> sync packet with hop data
#define RC_DATA_PACKET 0b00
#define MSP_DATA_PACKET 0b01
#define TLM_PACKET 0b11
#define SYNC_PACKET 0b10

// current and sent switch values
#define N_SWITCHES 8

class OTA
{
  // Define GenerateChannelData() function pointer
#ifdef ENABLE_TELEMETRY
  typedef void (*GenerateChannelDataFunc)(volatile uint8_t* Buffer,
                                          Channels* chan, bool TelemetryStatus);
#else
  typedef void (*GenerateChannelDataFunc)(volatile uint8_t* Buffer,
                                          Channels* chan);
#endif

  typedef void (*UnpackChannelDataFunc)(volatile uint8_t* Buffer,
                                        Channels* chan);

public:
  enum Mode { HybridSwitches8 = 0, Data10bit };

  GenerateChannelDataFunc GenerateChannelData;
  UnpackChannelDataFunc UnpackChannelData;

  // This could be called again if the config changes, for instance
  void init(Mode m);

  /**
   * Determine which switch to send next.
   * If any switch has changed since last sent, we send the lowest index changed
   * switch and set nextSwitchIndex to that value + 1. If no switches have
   * changed then we send nextSwitchIndex and increment the value. For pure
   * sequential switches, all 8 switches are part of the round-robin sequence.
   * For hybrid switches, switch 0 is sent with every packet and the rest of the
   * switches are in the round-robin.
   */
  uint8_t getNextSwitchIndex();

  void updateSwitchValues(Channels* chan);

private:
  friend void test_round_robin(void);
  friend void test_priority(void);
  friend void test_encodingHybrid8(bool highResChannel);
  friend void test_decodingHybrid8(uint8_t forceSwitch, uint8_t switchval);
  friend void test_encoding10bit();
  friend void test_decoding10bit();

  // stored switch values
  uint8_t CurrentSwitches[N_SWITCHES];

  // which switch should be sent in the next rc packet
  uint8_t NextSwitchIndex;

  void setSentSwitch(uint8_t index, uint8_t value);
  void setCurrentSwitch(uint8_t index, uint8_t value);

#ifdef ENABLE_TELEMETRY
  static void GenerateChannelData10bit(volatile uint8_t* Buffer, Channels* chan,
                                       bool TelemetryStatus);
  static void GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer,
                                               Channels* chan,
                                               bool TelemetryStatus);
#else
  static void GenerateChannelData10bit(volatile uint8_t* Buffer,
                                       Channels* chan);
  static void GenerateChannelDataHybridSwitch8(volatile uint8_t* Buffer,
                                               Channels* chan);
#endif

  static void UnpackChannelDataHybridSwitch8(volatile uint8_t* Buffer, Channels *chan);
  static void UnpackChannelData10bit(volatile uint8_t* Buffer, Channels *chan);
};

extern OTA ota;

#endif  // H_OTA
