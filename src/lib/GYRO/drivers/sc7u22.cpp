#include "targets.h"

#if defined(PLATFORM_ESP32)

#include <Arduino.h>
#include <Wire.h>

#include "logging.h"
#include "sc7u22.h"
#include "sc7u22_regs.h"


static bool SC7U22_detect(IMU_Driver *driver) {
    // Ensure the general register bank is selected before reading WHO_AM_I.
    driver->writeRegister(REG_SEG_SEL, 0x00, 10);

    for (uint8_t attempt = 0; attempt < 5; ++attempt)
    {
        uint8_t chipId = 0;
        if (driver->readRegister(REG_WHO_AM_I, &chipId, 1) && chipId == CHIP_ID)
        {
            return true;
        }
        delay(1);
    }
    
    return false;
}

static void SC7U22_configure(IMU_Driver *driver)
{
    // Start at 800hz
    driver->gyroSampleRate = 800;
    driver->period_us = 1000000 / driver->gyroSampleRate;

    driver->accScaleG = 4.0f / 32768.0f;
    driver->acc1G_adc = 32768.0f / 4.0f;
    driver->gyroScaleDeg = 2000.0f / 32768.0f;
    driver->gyroScaleRad = radians(driver->gyroScaleDeg);


    driver->writeRegister(REG_SEG_SEL, 0x00, 10);  // Segment Selection, normal Registers
    driver->writeRegister(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 10); // Mem-Reset, Block-Data-Update, Auto-Increment-addr

    // SC7U22 requires two soft reset writes for reliable startup.
    driver->writeRegister(REG_SOFT_RST, SOFT_RESET_VALUE, 1);
    driver->writeRegister(REG_SOFT_RST, SOFT_RESET_VALUE, 1);
    delay(RESET_DELAY_MS);

    driver->writeRegister(REG_SEG_SEL, 0x00, 10);  // Segment Selection, normal Registers
    driver->writeRegister(REG_COM_CONF, COM_CONF_BDU | COM_CONF_ADDR_AUTO, 10); // Block-Data-Update, Auto-Increment-addr

    driver->writeRegister(REG_INT1_OUT_SEL1, INT1_OUT_SEL1_GYR);   // Triger Int1 line when Gyro data is ready

    driver->writeRegister(REG_PWR_CTRL, 0x00, 1);               // Disable Temp, Gyro, ACC

    // Configure ACC
    driver->writeRegister(REG_ACC_RANGE, ACC_RANGE_4G, 1);         // ACC range: 4g
    driver->writeRegister(REG_ACC_CONF,
                            ACC_CONF_FILTER_PERF_HIGH | 
                            ACC_CONF_BWP_OSR4_AVG1 | ACC_CONF_ODR_800, 1); // Acc: Filter:   ODR:800,
                  
    // Configure Gyro
    driver->writeRegister(REG_GYR_RANGE, GYR_RANGE_2000DPS, 1);    // Gyro range: 2000dps
    driver->writeRegister(REG_GYR_CONF,
                            GYR_CONF_FILTER_PERF_HIGH | GYR_CONF_NOISE_PERF_HIGH | // High performance, Noise Optimization on
                            ACC_CONF_BWP_OSR4_AVG1 | GYR_CONF_ODR_800, 1); // Gyro: Filter:      ODR:800,
                  
    // Enable Gyro/Acc
    delay(5);
    driver->writeRegister(REG_PWR_CTRL,
                      PWR_CTRL_TEMP_EN | PWR_CTRL_ACC_EN | PWR_CTRL_GYR_EN); // Enable Temp, Gyro and ACC

    delay(SENSOR_START_DELAY_MS); 
}

static bool SC7U22_rawRead(IMU_Driver *driver,
                           int16_t *ax, int16_t *ay, int16_t *az,
                           int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t raw[12];
    if (!driver->readRegister(REG_ACC_XH, raw, sizeof(raw)))
    {
        *ax = *ay = *az = 0;
        *gx = *gy = *gz = 0;
        return false;
    }

    // SC7U22 acceleration and gyro output registers are signed big-endian.
    *ax = static_cast<int16_t>((static_cast<uint16_t>(raw[0]) << 8) | raw[1]);
    *ay = static_cast<int16_t>((static_cast<uint16_t>(raw[2]) << 8) | raw[3]);
    *az = static_cast<int16_t>((static_cast<uint16_t>(raw[4]) << 8) | raw[5]);
    *gx = static_cast<int16_t>((static_cast<uint16_t>(raw[6]) << 8) | raw[7]);
    *gy = static_cast<int16_t>((static_cast<uint16_t>(raw[8]) << 8) | raw[9]);
    *gz = static_cast<int16_t>((static_cast<uint16_t>(raw[10]) << 8) | raw[11]);
    return true;
}


const char *IMU_SC7U22_SPI::GetMPUName()
{
    return "SC7U22";
}

bool IMU_SC7U22_SPI::detect()
{
    if (SC7U22_detect(this)) 
    {
        return true;
    }

    return false;
}

bool IMU_SC7U22_SPI::configure()
{
    SC7U22_configure(this);
    return true;
}

bool IMU_SC7U22_SPI::initialize()
{
    //IMU_Driver_SPI::initialize();

// Initialize CS
    cs_pin = GPIO_PIN_GYRO_NSS;
    int_pin = GPIO_PIN_GYRO_INT;
    DBGLN("LSM6DXX(SPI) NSS Pin=%d, INT Pin=%d", cs_pin, int_pin);

    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    _spiSettings = SPISettings(10000000, MSBFIRST, SPI_MODE3);

    if (int_pin != UNDEF_PIN)
    {
        setupInterrupt(int_pin);
    }

    // Test The connection
    DBGLN("Detecting SC7U22 (SPI)");
    if (!detect())
    {
        DBGLN("SC7U22 not found!");
        return false;
    }
    
    configure();    
    DBGLN("SC7U22 (SPI) initialized");
    setInterruptReceived(true); // True to force read at lest the first time

    return true;
}

void IMU_SC7U22_SPI::start()
{
    DBGLN("SC7U22 Start");
}

bool IMU_SC7U22_SPI::isDataReady()
{
    const auto now = micros();
    static auto last_ready = micros();

    if (int_pin != UNDEF_PIN)
    {
        // Using Interrupt pin??
        bool ready = interruptReceived();
        if (!ready)
        {
            // Interrupts seems that sometimes they stop triggering,
            // if we can't read after  2x period, then force the read

            if (now > last_ready + period_us * 2)
            {
                non_ready_errors++;
                ready = true;
            }
        }
        else
        {
            last_ready = now;
        }
        return ready;
    }
    else
    {
        // Non-Interrupt, The device produces data faster than the configured 800 Hz polling rate.
        // AHRS::tick() enforces period_us before calling this method.
        return true;

    }
}
    

bool IMU_SC7U22_SPI::rawRead(int16_t *ax, int16_t *ay, int16_t *az,
                         int16_t *gx, int16_t *gy, int16_t *gz)
{
    setInterruptReceived(false);
    return SC7U22_rawRead(this, ax, ay, az, gx, gy, gz);
}


const char *IMU_SC7U22_I2C::GetMPUName()
{
    return "SC7U22";
}

bool IMU_SC7U22_I2C::detect()
{
    const uint8_t addresses[] = {I2C_ADDRESS_SDO_HIGH, I2C_ADDRESS_SDO_LOW};

    for (const uint8_t address : addresses)
    {
        m_address = address;

        if (SC7U22_detect(this)) 
        {
            DBGLN("SC7U22 found at I2C address 0x%x", m_address);
            return true;
        }
    }

    return false;
}

bool IMU_SC7U22_I2C::configure()
{
    SC7U22_configure(this);
    return true;
}

bool IMU_SC7U22_I2C::initialize()
{
    IMU_Driver_I2C::initialize();
    wire->setClock(400000);
    wire->setTimeOut(2);

    DBGLN("Detecting SC7U22");

    if (!detect())
    {
        DBGLN("SC7U22 not found!");
        return false;
    }

    configure();
    
    int16_t ax,ay,az,gx,gy,gz;
    int16_t ax1,ay1,az1;

    rawRead(&ax, &ay, &az, &gx, &gy, &gz);
    delay(10);
    rawRead(&ax1, &ay1, &az1, &gx, &gy, &gz);

    if ((ax==ax1) && (ay==ay1) && (az==az1)) 
    {
        DBGLN("SC7U22 failed to configure, trying again");
        configure();
    }


    DBGLN("SC7U22 initialized");
    return true;
}

void IMU_SC7U22_I2C::start()
{
    DBGLN("SC7U22 Start");
}

bool IMU_SC7U22_I2C::isDataReady()
{
    // The device produces data faster than the configured 800 Hz polling rate.
    // AHRS::tick() enforces period_us before calling this method.
    return true;
}

bool IMU_SC7U22_I2C::rawRead(int16_t *ax, int16_t *ay, int16_t *az,
                         int16_t *gx, int16_t *gy, int16_t *gz)
{
    return SC7U22_rawRead(this, ax, ay, az, gx, gy, gz);
}

#endif
