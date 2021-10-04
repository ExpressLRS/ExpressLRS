#include <Arduino.h>
#include "ESP8266_WebUpdate.h"
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "msp.h"
#include "msptypes.h"

/////////// DEFINES ///////////

#define WIFI_PIN          0
#define LED_PIN           16
#define EEPROM_ADDR_WIFI  0x00

/////////// GLOBALS ///////////


bool cacheFull = false;
bool sendCached = false;
uint8_t flashLED = false;
bool startWebUpdater = false;
uint8_t broadcastAddress[6] = {MY_UID};

/////////// CLASS OBJECTS ///////////

MSP msp;
mspPacket_t cachedVTXPacket;

/////////// FUNCTION DEFS ///////////

void ProcessMSPPacketFromPeer(mspPacket_t *packet);
void OnDataRecv(uint8_t * mac_addr, uint8_t *data, uint8_t data_len);
void sendMSPViaEspnow(mspPacket_t *packet);
void ProcessMSPPacketFromTX(mspPacket_t *packet);
void SetSoftMACAddress();

/////////////////////////////////////

void RebootIntoWifi()
{
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

void ProcessMSPPacketFromPeer(mspPacket_t *packet)
{
  if (packet->function == MSP_ELRS_REQU_VTX_PKT)
  {
    // request from the vrx-backpack to send cached VTX packet
    if (cacheFull)
    {
      sendCached = true;
    }
  }
}

// espnow on-receive callback
void OnDataRecv(uint8_t * mac_addr, uint8_t *data, uint8_t data_len)
{
  for(int i = 0; i < data_len; i++)
  {
    if (msp.processReceivedByte(data[i]))
    {
      // Finished processing a complete packet
      ProcessMSPPacketFromPeer(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }
  flashLED = true;
}

void ProcessMSPPacketFromTX(mspPacket_t *packet)
{
  // If we just got a VTX packet, cache it for re-sending later (i.e if the goggles are off)
  if (packet->function == MSP_SET_VTX_CONFIG)
  {
    cachedVTXPacket = *packet;
    cacheFull = true;
  }
  
  // transparently forward MSP packets via espnow to any subscribers
  sendMSPViaEspnow(packet);
}

void sendMSPViaEspnow(mspPacket_t *packet)
{
  uint8_t packetSize = msp.getTotalPacketSize(packet);
  uint8_t nowDataOutput[packetSize];

  uint8_t result = msp.convertToByteArray(packet, nowDataOutput);

  if (!result)
  {
    // packet could not be converted to array, bail out
    return;
  }
  
  esp_now_send(broadcastAddress, (uint8_t *) &nowDataOutput, packetSize);
}

void SendCachedMSP()
{
  if (!cacheFull)
  {
    // nothing to send
    return;
  }

  sendMSPViaEspnow(&cachedVTXPacket);
}

// MAC address can only be set with unicast, so first byte must be even, not odd
void checkBroadcastAddressIsUnicast()
{
  if (broadcastAddress[0] % 2)
  {
    broadcastAddress[0] += 1;
  }
}

void SetSoftMACAddress()
{
  checkBroadcastAddressIsUnicast();

  WiFi.mode(WIFI_STA);

  // Soft-set the MAC address to the passphrase UID for binding
  wifi_set_macaddr(STATION_IF, broadcastAddress);
}

void setup()
{
  Serial.begin(460800);

  SetSoftMACAddress();

  EEPROM.begin(512);
  EEPROM.get(0, startWebUpdater);
  
  if (startWebUpdater)
  {
    EEPROM.put(0, false);
    EEPROM.commit();  
    BeginWebUpdate();
  }
  else
  {
    WiFi.mode(WIFI_STA);

    if (esp_now_init() != 0)
    {
      // Error initializing ESP-NOW
      ESP.restart();
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
    esp_now_register_recv_cb(OnDataRecv);
  }

  pinMode(WIFI_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  flashLED = true;
} 

void loop()
{  
  // press the boot button to start webupdater
  if (!digitalRead(WIFI_PIN))
  {
    RebootIntoWifi();
  }
  
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

  if (Serial.available())
  {
    uint8_t c = Serial.read();

    if (msp.processReceivedByte(c))
    {
      // Finished processing a complete packet
      ProcessMSPPacketFromTX(msp.getReceivedPacket());
      msp.markPacketReceived();
    }
  }

  if (cacheFull && sendCached)
  {
    SendCachedMSP();
    sendCached = false;
  }
}
