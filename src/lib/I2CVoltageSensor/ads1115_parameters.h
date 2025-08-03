#pragma once
#include "common.h"
#include "device.h"
#include "logging.h"

#include <cstdint>
// The ADS1115 allows up to four different addresses based on the wiring of the chip.
// The selection works based on what the AADR pin is connected to thus the enum fields will carry those names.
enum class ADS1115Address : uint8_t
{
    GND = 0b1001000,
    VDD = 0b1001001,
    SDA = 0b1001010,
    SCL = 0b1001011,
};

enum class ADS1115Register : uint8_t
{
    Conversion = 0b00,
    Config = 0b01,
    Lo_thresh = 0b10,
    Hi_thresh = 0b11,
};

enum class ADS1115DataRate : uint8_t
{
    _8SPS = 0b000,
    _16SPS = 0b001,
    _32SPS = 0b010,
    _64SPS = 0b011,
    _128SPS = 0b100,
    _250SPS = 0b101,
    _475SPS = 0b110,
    _860SPS = 0b111,
};

inline ADS1115DataRate ADS1115DataRateFromUint8(const uint8_t value)
{
    switch (value)
    {
    case 0b000:
        return ADS1115DataRate::_8SPS;
    case 0b001:
        return ADS1115DataRate::_16SPS;
    case 0b010:
        return ADS1115DataRate::_32SPS;
    case 0b011:
        return ADS1115DataRate::_64SPS;
    case 0b100:
        return ADS1115DataRate::_128SPS;
    case 0b101:
        return ADS1115DataRate::_250SPS;
    case 0b110:
        return ADS1115DataRate::_475SPS;
    case 0b111:
        return ADS1115DataRate::_860SPS;
    }
    return ADS1115DataRate::_32SPS;
}

inline int ADS1115DataRateToUpdateTimeMS(const ADS1115DataRate &data)
{
	// Assumption: a single-shot sample also finishes earlier if the
	// sample rate goes up.
    switch (data)
    {
    case ADS1115DataRate::_8SPS:
        return 1000 / 8;
    case ADS1115DataRate::_16SPS:
        return 1000 / 16;
    case ADS1115DataRate::_32SPS:
        return 1000 / 32;
    case ADS1115DataRate::_64SPS:
        return 1000 / 64;
    case ADS1115DataRate::_128SPS:
        return 1000 / 128;
    case ADS1115DataRate::_250SPS:
        return 1000 / 250;
    case ADS1115DataRate::_475SPS:
        return 1000 / 475;
    case ADS1115DataRate::_860SPS:
        return 1000 / 860;
    }

    return 1000;
}

inline uint8_t ADS1115DataRateToUint8(const ADS1115DataRate &rate)
{
    return static_cast<uint8_t>(rate);
}

enum class ADS1115PGAFSR : uint8_t
{
    _6_144V = 0b000,
    _4_096V = 0b001,
    _2_048V = 0b010,
    _1_024V = 0b011,
    _0_512V = 0b100,
    _0_256V = 0b101,

    // aliases of the _0_256V variant
    _0_256V_2 = 0b110,
    _0_256V_3 = 0b111,
};

inline uint8_t ADS1115PGAFSRToUint8(const ADS1115PGAFSR &gain)
{
    return static_cast<uint8_t>(gain);
}

enum class ADS1115Mode : uint8_t
{
    CONTINOUS = 0,
    SINGLE = 1,
};

inline uint8_t ADS1115ModeToUint8(const ADS1115Mode &mode)
{
    return static_cast<uint8_t>(mode);
}

enum class ADS1115MuxInput : uint8_t
{
    // format: {AINp}_{AINn}
    AIN0_AIN1 = 0b000,
    AIN0_AIN3 = 0b001,
    AIN1_AIN3 = 0b010,
    AIN2_AIN3 = 0b011,
    AIN0_GND = 0b100,
    AIN1_GND = 0b101,
    AIN2_GND = 0b110,
    AIN3_GND = 0b111,

    // Default value
    DEFAULT_INPUT = AIN0_AIN1,
};

inline uint8_t ADS1115MuxInputToUint8(const ADS1115MuxInput &input)
{
    return static_cast<uint8_t>(input);
}

typedef struct
{
    uint8_t comp_que : 2;
    uint8_t comp_pol : 1;
    uint8_t comp_lat : 1;
    uint8_t comp_mode : 1;
    uint8_t dr : 3;

    uint8_t mode : 1;
    uint8_t pga : 3;
    uint8_t mux : 3;
    uint8_t os : 1;

#if defined(DEBUG_LOG)
    void prettyPrint() const
    {
        DBGLN("Configuration: ");
        DBGLN("\tos: %x", os);
        DBGLN("\tmux: %x", mux);
        DBGLN("\tpga: %x", pga);
        DBGLN("\tmode: %x", mode);
        DBGLN("\tdr: %x", dr);
        DBGLN("\tcomp_mode: %x", comp_mode);
        DBGLN("\tcomp_pol: %x", comp_pol);
        DBGLN("\tcomp_lat: %x", comp_lat);
        DBGLN("\tcomp_que: %x", comp_que);
    }
#endif
} __attribute__((packed)) ADS1115_config_register_t;

typedef union {
    uint16_t val;
    ADS1115_config_register_t fields;
} ADS1115_config_register_union_t;
