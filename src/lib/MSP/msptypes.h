#pragma once

#define MSP_ELRS_FUNC       0x4578 // ['E','x']

// ELRS specific opcodes
#define MSP_ELRS_RF_MODE    0x00
#define MSP_ELRS_TX_PWR     0x01
#define MSP_ELRS_TLM_RATE   0x02

// CRSF encapsulated msp defines
#define ENCAPSULATED_MSP_PAYLOAD_SIZE 4

#define TELEMETRY_MSP_VERSION    1
#define TELEMETRY_MSP_VER_SHIFT  5
#define TELEMETRY_MSP_VER_MASK   (0x7 << TELEMETRY_MSP_VER_SHIFT)
#define TELEMETRY_MSP_START_FLAG (1 << 4)
#define TELEMETRY_MSP_SEQ_MASK   0x0F

typedef struct {
    uint8_t     header;
    uint8_t     payloadSize;
    uint8_t     function;
    uint8_t     payload[ENCAPSULATED_MSP_PAYLOAD_SIZE];
    uint8_t     crc;

    void reset()
    {
        header = 0;
        function = 0;
        payloadSize = 0;
        crc = 0;
    }

    bool addByte(uint8_t b)
    {
        if (payloadSize >= ENCAPSULATED_MSP_PAYLOAD_SIZE) {
          Serial.println("Failed to add byte to MSP payload - size overrun");
          return false;
        }
        payload[payloadSize++] = b;
        return true;
    }

    void setSeqNumber(uint8_t seq)
    {
        // Clear current seq
        header &= ~TELEMETRY_MSP_SEQ_MASK;
        // Set new seq
        header |= seq & TELEMETRY_MSP_SEQ_MASK;
    }

    void setVersion(uint8_t ver)
    {
        // Clear current version
        header &= ~TELEMETRY_MSP_VER_MASK;
        // Set new version
        header |= (ver << TELEMETRY_MSP_VER_SHIFT) & TELEMETRY_MSP_VER_MASK;
    }

    void setStartingFlag(bool isStartPacket)
    {
        // Set new flag
        if (isStartPacket) {
          header |= TELEMETRY_MSP_START_FLAG;
        }
        else {
          header &= ~TELEMETRY_MSP_START_FLAG;
        }
    }

    void calcCRC()
    {
        crc = payloadSize ^ function;
        for (uint8_t i = 0; i < ENCAPSULATED_MSP_PAYLOAD_SIZE; ++i) {
          crc ^= payload[i];
        }
    }

    uint8_t getTotalSize()
    {
        return payloadSize + sizeof(header) + sizeof(function) + sizeof(payloadSize) + sizeof(crc);
    }
} __attribute__ ((packed)) encapsulatedMspPacket_t;
