#include "rapidfire.h"
#include <SPI.h>

void
Rapidfire::Init()
{
    delay(2000);  // wait for rapidfire to boot

    // Set pins to output to configure rapidfire in SPI mode
    pinMode(PIN_MOSI, OUTPUT);
    pinMode(PIN_CLK, OUTPUT);
    pinMode(PIN_CS, OUTPUT);

    // put the RF into SPI mode by pulling all 3 pins high,
    // then low within 100-400ms
    digitalWrite(PIN_MOSI, HIGH);
    digitalWrite(PIN_CLK, HIGH);
    digitalWrite(PIN_CS, HIGH);
    delay(200);
    digitalWrite(PIN_MOSI, LOW);
    digitalWrite(PIN_CLK, LOW);
    digitalWrite(PIN_CS, LOW);

    SPI.begin();

    Serial.println("SPI config complete");
}

void
Rapidfire::SendBuzzerCmd()
{
    Serial.print("Beep!");
    
    uint8_t cmd[4];
    cmd[0] = RF_API_BEEP_CMD;   // 'S'
    cmd[1] = RF_API_DIR_GRTHAN; // '>'
    cmd[2] = 0x00;              // len
    cmd[3] = crc8(cmd, 3);

    SendSPI(cmd, 4);
}

void
Rapidfire::SendChannelCmd(uint8_t channel)
{
    Serial.print("Setting channel ");
    Serial.println(channel);

    uint8_t cmd[5];
    cmd[0] = RF_API_CHANNEL_CMD;    // 'C'
    cmd[1] = RF_API_DIR_EQUAL;      // '='
    cmd[2] = 0x01;                  // len
    cmd[3] = channel;               // temporarily set byte 4 to channel for crc calc
    cmd[3] = crc8(cmd, 4);          // reset byte 4 to crc
    cmd[4] = channel;               // assign channel to correct byte 5

    SendSPI(cmd, 5);
}

void
Rapidfire::SendBandCmd(uint8_t band)
{
    Serial.print("Setting band ");
    Serial.println(band);

    // 0x01 - ImmersionRC/FatShark
    // 0x02 - RaceBand
    // 0x03 - Boscam E
    // 0x04 - Boscam B
    // 0x05 - Boscam A
    // 0x06 - LowRace
    // 0x07 - Band X

    uint8_t cmd[5];
    cmd[0] = RF_API_BAND_CMD;       // 'C'
    cmd[1] = RF_API_DIR_EQUAL;      // '='
    cmd[2] = 0x01;                  // len
    cmd[3] = band;                  // temporarily set byte 4 to band for crc calc
    cmd[3] = crc8(cmd, 4);          // reset byte 4 to crc
    cmd[4] = band;                  // assign band to correct byte 5

    SendSPI(cmd, 5);
}

void
Rapidfire::SendSPI(uint8_t* buf, uint8_t bufLen)
{
    digitalWrite(PIN_CS, LOW);
    delay(100); // these delays might not be required. Came from example code

    SPI.beginTransaction(SPISettings(40000, MSBFIRST, SPI_MODE0));

    // debug code for printing SPI pkt
    for (int i = 0; i < bufLen; ++i)
    {
        Serial.print(buf[i], HEX);
        Serial.print(",");
    }
    Serial.println("");
    
    SPI.transfer(buf, bufLen);  // send API cmd to rapidfire

    SPI.endTransaction();

    digitalWrite(PIN_CS, HIGH);
    delay(100);
}

// CRC function for IMRC rapidfire API
// Input: byte array, array length
// Output: crc byte
uint8_t
Rapidfire::crc8(uint8_t* buf, uint8_t bufLen)
{
  uint32_t sum = 0;
  for (uint8_t i = 0; i < bufLen; ++i)
  {
    sum += buf[i];
  }
  return sum & 0xFF;
}
