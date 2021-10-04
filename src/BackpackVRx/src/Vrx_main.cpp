#include <Arduino.h>
#include "ESP8266_WebUpdate.h"
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "msp.h"
#include "msptypes.h"
#include "config.h"

extern "C" {
  #include <user_interface.h>
}

#ifdef RAPIDFIRE_BACKPACK
  #include "rapidfire.h"
#endif

#define WIFI_PIN            0
#define LED_PIN             16

#define OPCODE_SET_CHANNEL  0x01
#define OPCODE_SET_BAND     0x02
#define OPCODE_WIFI_MODE    0x0F

#define EEPROM_ADDR_WIFI    0x00

uint8_t broadcastAddress[6] = {MY_UID};

bool startWebUpdater = false;
uint8_t flashLED = false;

uint8_t cachedBand = 0;
uint8_t cachedChannel = 0;
bool sendChanges = false;

MSP msp;

#ifdef RAPIDFIRE_BACKPACK
  Rapidfire vrxModule;
#elif GENERIC_BACKPACK
  // other VRx backpack (i.e. reserved for FENIX or fusion etc.)
#endif

/////////// FUNCTION DEFS ///////////

void RebootIntoWifi();
void OnDataRecv(uint8_t * mac_addr, uint8_t *data, uint8_t data_len);
void ProcessMSPPacket(mspPacket_t *packet);
void SetSoftMACAddress();

/////////////////////////////////////

void RebootIntoWifi()
{
  Serial.println("Rebooting into wifi update mode...");

  startWebUpdater = true;
  EEPROM.put(EEPROM_ADDR_WIFI, startWebUpdater);
  EEPROM.commit();
  
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(LED_PIN, LOW);
    delay(50);
    digitalWrite(LED_PIN, HIGH);
    delay(50);
  }

  delay(500);
  ESP.restart();
}

// espnow on-receive callback
void OnDataRecv(uint8_t * mac_addr, uint8_t *data, uint8_t data_len)
{
  Serial.println("ESP NOW DATA:");
  for(int i = 0; i < data_len; i++)
  {
    Serial.print(data[i], HEX); // Debug prints
    Serial.print(",");

    if (msp.processReceivedByte(data[i]))
    {
      // Finished processing a complete packet
      ProcessMSPPacket(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }
}

void ProcessMSPPacket(mspPacket_t *packet)
{
  if (packet->function == MSP_SET_VTX_CONFIG)
  {
    if (packet->payload[0] < 48) // Standard 48 channel VTx table size e.g. A, B, E, F, R, L
    {
      // only send new changes to the goggles
      // cache changes here, to be handled outside this callback, in the main loop
      uint8_t newBand = packet->payload[0] / 8 + 1;
      uint8_t newChannel = packet->payload[0] % 8;
      if (cachedBand != newBand)
      {
        cachedBand = newBand;
        sendChanges = true;
      }
      if (cachedChannel != newChannel)
      {
        cachedChannel = newChannel;
        sendChanges = true;
      }

      if (cachedBand == 6)
      {
        RebootIntoWifi();   // testing only, remove later!
      }
    }
    else
    {
      return; // Packets containing frequency in MHz are not yet supported.
    }
  }
  else if (packet->function == MSP_ELRS_SET_WIFI_MODE)
  {
    RebootIntoWifi();
  }
}

void SetSoftMACAddress()
{
  WiFi.mode(WIFI_STA);

  // Soft-set the MAC address to the passphrase UID for binding
  wifi_set_macaddr(STATION_IF, broadcastAddress);
}

void setup()
{
  Serial.begin(460800);

  broadcastAddress[0] = 0; // MAC address can only be set with unicast, so first byte must be even, not odd

  SetSoftMACAddress();

  EEPROM.begin(512);
  EEPROM.get(EEPROM_ADDR_WIFI, startWebUpdater);

  if (startWebUpdater)
  {
    EEPROM.put(EEPROM_ADDR_WIFI, false);
    EEPROM.commit();  
    BeginWebUpdate();
  }
  else
  {
    WiFi.mode(WIFI_STA);
    Serial.print("after mode MAC Address: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != 0) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    esp_now_register_recv_cb(OnDataRecv); 
  }

  vrxModule.Init();

  pinMode(WIFI_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  flashLED = true;
  Serial.println("Setup completed");
}

void loop()
{
  if (startWebUpdater)
  {
    HandleWebUpdate();
    flashLED = true;
  }
  
  if (flashLED)
  {
    flashLED = false;
    for (int i = 0; i < 4; i++)
    {
      digitalWrite(LED_PIN, LOW);
      startWebUpdater == true ? delay(50) : delay(200);
      digitalWrite(LED_PIN, HIGH);
      startWebUpdater == true ? delay(50) : delay(200);
    }
  }

  if (sendChanges)
  {
    sendChanges = false;
    // rapidfire sometimes misses pkts, so send each one 3x
    vrxModule.SendBandCmd(cachedBand);
    vrxModule.SendBandCmd(cachedBand);
    vrxModule.SendBandCmd(cachedBand);

    vrxModule.SendChannelCmd(cachedChannel);
    vrxModule.SendChannelCmd(cachedChannel);
    vrxModule.SendChannelCmd(cachedChannel);
  }
}
