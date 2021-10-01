#include <Arduino.h>
#include "ESP8266_WebUpdate.h"
#include <espnow.h>
#include <EEPROM.h>

#define WIFI_PIN 0
#define LED_PIN 16

#define OPCODE_SET_CHANNEL  0x01
#define OPCODE_SET_BAND     0x02
#define OPCODE_WIFI_MODE    0x0F

uint8_t flashLED = false;

bool startWebUpdater = false;
uint8_t channelHistory[3] = {255};

uint8_t broadcastAddress[] = {TX_MAC};  // r9 tx    50:02:91:DA:37:84

int channel = 1;  // testing only

void OnDataRecv(uint8_t * mac_addr, uint8_t *data, uint8_t data_len)
{
  for(int i=0; i<data_len; i++)
  {
    Serial.write(data[i]);
  }

  channelHistory[2] = channelHistory[1];
  channelHistory[1] = channelHistory[0];
  channelHistory[0] = data[8];
    
  flashLED = true;
}

void sendVRXChannelCmd(uint8_t channel)
{
    uint8_t nowDataOutput[2];

    nowDataOutput[0] = OPCODE_SET_CHANNEL;
    nowDataOutput[1] = channel;

    Serial.println("sending channel change...");
    
    esp_now_send(broadcastAddress, (uint8_t *) &nowDataOutput, sizeof(nowDataOutput));
}

void sendVRXBandCmd(uint8_t band)
{
    uint8_t nowDataOutput[2];

    nowDataOutput[0] = OPCODE_SET_BAND;
    nowDataOutput[1] = band;

    Serial.println("sending band change...");
    
    esp_now_send(broadcastAddress, (uint8_t *) &nowDataOutput, sizeof(nowDataOutput));
}

void sendVRXWifiCmd()
{
    uint8_t nowDataOutput[1];

    nowDataOutput[0] = OPCODE_WIFI_MODE;

    Serial.println("sending wifi cmd...");
    
    esp_now_send(broadcastAddress, (uint8_t *) &nowDataOutput, sizeof(nowDataOutput));
}

void setup()
{
  Serial.begin(460800);

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

    if(esp_now_init() != 0)
    {
      ESP.restart();
    }

    esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

    // esp_now_register_recv_cb(OnDataRecv); 
  }

  pinMode(WIFI_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  
  flashLED = true;
} 

void loop()
{  
  // A1, A2, A1 Select this channel order to start webupdater
  // Or press the boot button
  if ( (channelHistory[0] == 0 && channelHistory[1] == 1 && channelHistory[2] == 0) || !digitalRead(WIFI_PIN) )
  {
    EEPROM.put(0, true);
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

  // test code
  sendVRXChannelCmd(channel);
  delay(1000);
  sendVRXBandCmd(0x01); // fatshark

  if (++channel > 8)
  {
    channel = 1;
  }

  if (millis() > 30000)
  {
    // test wifi cmd after 30s of uptime
    sendVRXWifiCmd();
  }

  delay(1000);
}