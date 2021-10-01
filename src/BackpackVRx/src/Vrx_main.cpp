#include <Arduino.h>
#include "ESP8266_WebUpdate.h"
#include <espnow.h>
#include <EEPROM.h>

#ifdef RAPIDFIRE_BACKPACK
  #include "rapidfire.h"
#endif

/////////////// OLD CODE: ////////////////////

// #define ADDRESS_BITS        0x0F
// #define DATA_BITS           0xFFFFF
// #define SPI_ADDRESS_SYNTH_B 0x01
// #define MSP_SET_VTX_CONFIG  89 //in message          Set vtx settings - betaflight

// uint32_t spiData = 0;
// bool mosiVal = false;
// uint8_t numberOfBitsReceived = 0; // Takes into account 25b and 32b packets
// bool gotData = false;

// uint8_t channelHistory[3] = {255};

#define WIFI_PIN            0
#define LED_PIN             16

#define OPCODE_SET_CHANNEL  0x01
#define OPCODE_SET_BAND     0x02
#define OPCODE_WIFI_MODE    0x0F

#define EEPROM_ADDR_WIFI    0x00

uint8_t broadcastAddress[] = {TX_MAC};  // r9 tx    50:02:91:DA:37:84

bool startWebUpdater = false;
uint8_t flashLED = false;

#ifdef RAPIDFIRE_BACKPACK
  Rapidfire vrxModule;
#elif VRX_BACKPACK
  // other VRx backpack (i.e. reserved for FENIX or fusion etc.)
#endif

// uint8_t crc8_dvb_s2(uint8_t crc, unsigned char a)
// {
//   crc ^= a;
//   for (int ii = 0; ii < 8; ++ii) {
//       if (crc & 0x80) {
//           crc = (crc << 1) ^ 0xD5;
//       } else {
//           crc = crc << 1;
//       }
//   }
//   return crc;
// }

// void IRAM_ATTR spi_clk_isr();
// void IRAM_ATTR spi_cs_isr();
// void IRAM_ATTR spi_mosi_isr();

// void IRAM_ATTR spi_cs_isr()
// {
//   if (digitalRead(PIN_CS) == HIGH)
//   {
//     spiData = spiData >> (32 - numberOfBitsReceived);
//     gotData = true;
//   }
//   else
//   {
//     spiData = 0;
//     numberOfBitsReceived = 0;
//   }
// }

// void IRAM_ATTR spi_mosi_isr()
// {
//   mosiVal = digitalRead(PIN_MOSI);
// }

// void IRAM_ATTR spi_clk_isr()
// {
//   spiData = spiData >> 1;
//   spiData = spiData | (mosiVal << 31);
//   numberOfBitsReceived++;
// }

// void sendToExLRS(uint16_t function, uint16_t payloadSize, const uint8_t *payload)
// {
//     uint8_t nowDataOutput[9 + 4];

//     nowDataOutput[0] = '$';
//     nowDataOutput[1] = 'X';
//     nowDataOutput[2] = '<';
//     nowDataOutput[3] = '0';
//     nowDataOutput[4] = function & 0xff;
//     nowDataOutput[5] = (function >> 8) & 0xff;
//     nowDataOutput[6] = payloadSize & 0xff;
//     nowDataOutput[7] = (payloadSize >> 8) & 0xff;

//     for (int i = 0; i < payloadSize; i++)
//     {
//         nowDataOutput[8 + i] = payload[i];
//     }

//     uint8_t ck2 = 0;
//     for(int i = 3; i < payloadSize+8; i++)
//     {
//         ck2=crc8_dvb_s2(ck2, nowDataOutput[i]);
//     }
//     nowDataOutput[payloadSize+8] = ck2;

//     esp_now_send(broadcastAddress, (uint8_t *) &nowDataOutput, sizeof(nowDataOutput));

//     flashLED = true;
// }

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
  // debug print for incomming data
  Serial.println("ESP NOW DATA:");
  for(int i = 0; i < data_len; i++)
  {
    Serial.println(data[i]);
  }

  uint8_t opcode = data[0];
  uint8_t channel = 0;
  uint8_t band = 0;

  switch (opcode)
  {
  case OPCODE_SET_CHANNEL:
    channel = data[1];
    vrxModule.SendChannelCmd(channel);
    break;
  case OPCODE_SET_BAND:
    band = data[1];
    vrxModule.SendBandCmd(band);
    break;
  case OPCODE_WIFI_MODE:
    RebootIntoWifi();
    break;
  default:
    Serial.println("Unknown opcode received!");
    break;
  }
} 

void setup()
{
  Serial.begin(460800);

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

    // esp_now_set_self_role(ESP_NOW_ROLE_CONTROLLER);
    // esp_now_add_peer(broadcastAddress, ESP_NOW_ROLE_SLAVE, 1, NULL, 0);

    esp_now_register_recv_cb(OnDataRecv); 

    // delay(1000);
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
}
