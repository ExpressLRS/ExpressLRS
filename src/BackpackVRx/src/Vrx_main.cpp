#include <Arduino.h>
#include "ESP8266_WebUpdate.h"
#include <espnow.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include "msp.h"
#include "msptypes.h"

#ifdef RAPIDFIRE_BACKPACK
  #include "rapidfire.h"
#endif

/////////// DEFINES ///////////

#define WIFI_PIN            0
#define LED_PIN             16
#define EEPROM_ADDR_WIFI    0x00

/////////// GLOBALS ///////////

uint8_t broadcastAddress[6] = {MY_UID};

bool startWebUpdater = false;
uint8_t flashLED = false;

uint8_t cachedBand = 0;
uint8_t cachedChannel = 0;
bool sendChangesToVrx = false;
bool gotInitialPacket = false;
uint32_t lastSentRequest = 0;

/////////// CLASS OBJECTS ///////////

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
void RequestVTXPacket();
void sendMSPViaEspnow(mspPacket_t *packet);

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
    gotInitialPacket = true;
    if (packet->payload[0] < 48) // Standard 48 channel VTx table size e.g. A, B, E, F, R, L
    {
      // only send new changes to the goggles
      // cache changes here, to be handled outside this callback, in the main loop
      uint8_t newBand = packet->payload[0] / 8 + 1;
      uint8_t newChannel = packet->payload[0] % 8;
      if (cachedBand != newBand)
      {
        cachedBand = newBand;
        sendChangesToVrx = true;
      }
      if (cachedChannel != newChannel)
      {
        cachedChannel = newChannel;
        sendChangesToVrx = true;
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

void RequestVTXPacket()
{  
  mspPacket_t packet;
  packet.reset();
  packet.makeCommand();
  packet.function = MSP_ELRS_REQU_VTX_PKT;
  packet.addByte(0);  // empty byte

  sendMSPViaEspnow(&packet);
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

void setup()
{
  Serial.begin(460800);

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

    if (esp_now_init() != 0) {
      Serial.println("Error initializing ESP-NOW");
      return;
    }

    esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
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

  if (sendChangesToVrx)
  {
    sendChangesToVrx = false;
    // rapidfire sometimes misses pkts, so send each one 3x
    vrxModule.SendBandCmd(cachedBand);
    vrxModule.SendBandCmd(cachedBand);
    vrxModule.SendBandCmd(cachedBand);

    vrxModule.SendChannelCmd(cachedChannel);
    vrxModule.SendChannelCmd(cachedChannel);
    vrxModule.SendChannelCmd(cachedChannel);
  }

  // spam out a bunch of requests for the desired band/channel for the first 5s
  if (!gotInitialPacket && millis() < 5000 && millis() - lastSentRequest > 1000)
  {
    RequestVTXPacket();
    lastSentRequest = millis();
  }
}
