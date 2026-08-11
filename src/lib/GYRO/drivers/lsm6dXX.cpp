#include "targets.h"

#if defined(PLATFORM_ESP32)
#include "logging.h"
#include "lsm6dXX.h"
#include "lsm6dxx_regs.h"

#define LSM6DSV16X_ADDRESS_LOW 0x6A // Default
#define LSM6DSV16X_ADDRESS_HIGH 0x6B

static uint8_t accScaleCode, gyroScaleCode;

static uint8_t lsm6dID = 0;

/******************************
** Read STATUS_REG to get DataReady
*******************************/
static bool lsm6dxxAccGyroDataReady(IMU_Driver *driver)
{
    uint8_t status = driver->readRegister(LSM6DXX_REG_STATUS);
    // Check Accel (XL) and Gyro data available
    uint8_t bits = LSM6DXX_VAL_STATUS_XLDA | LSM6DXX_VAL_STATUS_GDA;
    return ((status & bits) == bits); // Has Gyro and Acce;
}

/*********************************
 * Detect chip
 **********************************/
static bool lsm6dxxDetect(IMU_Driver *driver)
{
    // Read chip ID
    uint8_t id = driver->readRegister(LSM6DXX_REG_WHO_AM_I);
    DBGLN("Gyro Id returned = 0x%x%x", id);
    if (id != LSM6DSO_CHIP_ID && id != LSM6DSL_CHIP_ID)
    { // 0x6C=LSM6DSO, 0x6A=LSM6DSL
        return false;
    }

    lsm6dID = id;
    return true;
}

static void lsm6dxxConfig(IMU_Driver *driver)
{
    // Reset the device
    driver->writeRegister(LSM6DXX_REG_CTRL3_C, 0x01); // SW_RESET
    delay(100);

    // Verify reset by reading back
    // uint8_t ctrl3 = driver->readRegister(LSM6DXX_REG_CTRL3_C);
    // DBGLN("SPI DEV CTRL3_C after reset: 0x%x", ctrl3);

    // Configure interrupt pin 1 for gyro data ready only
    driver->writeRegister(LSM6DXX_REG_INT1_CTRL, LSM6DXX_VAL_INT1_CTRL_ENABLE);

    // Disable interrupt pin 2
    driver->writeRegister(LSM6DXX_REG_INT2_CTRL, LSM6DXX_VAL_INT2_CTRL_DISABLE);

    // Configure accelerometer:
    uint8_t data = (LSM6DXX_VAL_CTRL1_XL_ODR833 << 4) | // ODR 1.6KHZ,  Original ODR 833Hz
                   (LSM6DXX_VAL_CTRL1_XL_16G << 2) |    // 16G Scale
                   (LSM6DXX_VAL_CTRL1_XL_LPF1 << 1);    // Use output from LPF1

    driver->writeRegister(LSM6DXX_REG_CTRL1_XL, data);

    // Configure gyro: ODR 833hz, ±2000dps
    driver->writeRegister(LSM6DXX_REG_CTRL2_G,
                          (LSM6DXX_VAL_CTRL2_G_ODR833 << 4) |      // ODR 833hz
                              (LSM6DXX_VAL_CTRL2_G_2000DPS << 2)); // 2000dps scale

    // Configure control register 3
    // BDU: latch LSB/MSB during reads; prevents MSB from being updated while burst reading LSB/MSB
    // set interrupt pins active high (def); set interrupt pins push/pull (def); set 4-wire SPI (def);
    // enable auto-increment burst reads
    driver->writeRegister(LSM6DXX_REG_CTRL3_C,
                          LSM6DXX_VAL_CTRL3_C_BDU |                                                                 // output registers are not updated until MSB and LSB have been read (prevents MSB from being updated while burst reading LSB/MSB)
                              LSM6DXX_VAL_CTRL3_C_H_LACTIVE | LSM6DXX_VAL_CTRL3_C_PP_OD | LSM6DXX_VAL_CTRL3_C_SIM | // Def 0s
                              LSM6DXX_VAL_CTRL3_C_IF_INC);                                                          // enable auto-increment burst reads

    // Configure control register 4
    driver->writeRegister(LSM6DXX_REG_CTRL4_C,
                          (LSM6DXX_VAL_CTRL4_C_DRDY_ENABLED | // enable accelerometer high performane mode;
                           LSM6DXX_VAL_CTRL4_C_I2C_DISABLE |  // Disable I2C
                           LSM6DXX_VAL_CTRL4_C_LPF1_SEL_G));  // enable gyro LPF1

    // Configure control register 6 for Low Pass Filter (LPF1)
    driver->writeRegister(LSM6DXX_REG_CTRL6_C,
                          (LSM6DXX_VAL_CTRL6_C_XL_HM_MODE | // High Performance Mode
                                                            // LSM6DXX_VAL_CTRL6_C_FTYPE_99HZ // set gyro LPF1 cutoff 99Hz
                           LSM6DXX_VAL_CTRL6_C_FTYPE_171HZ  // set gyro LPF1 cutoff 171Hz
                           ));

    // NEW: Configure control register 7
    // Set High Pass Filters for Accelerometer
    // Not needed
    // writeReg(LSM6DXX_REG_CTRL7_G,
    //    (LSM6DXX_VAL_CTRL7_G_HP_EN_G |    // enable gyro high-pass filter
    //     LSM6DXX_VAL_CTRL7_G_HPM_G_16));  // gyro HPF cutoff 16mHz

    // Configure control register 9
    if (lsm6dID == LSM6DSO_CHIP_ID)
    {
        driver->writeRegister(LSM6DXX_REG_CTRL9_XL,
                              LSM6DXX_VAL_CTRL9_XL_I3C_DISABLE); // disable I3C interface
    }
}

static bool lsm6dxxAccGyroRead(IMU_Driver *driver, int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t data[12];

    driver->readRegister(LSM6DXX_REG_OUTX_L_G, data, 12);

    *gx = data[0] | static_cast<uint16_t>(data[1] << 8);
    *gy = data[2] | static_cast<uint16_t>(data[3] << 8);
    *gz = data[4] | static_cast<uint16_t>(data[5] << 8);

    *ax = data[6] | static_cast<uint16_t>(data[7] << 8);
    *ay = data[8] | static_cast<uint16_t>(data[9] << 8);
    *az = data[10] | static_cast<uint16_t>(data[11] << 8);
    return true;
}

static bool lsm6dxxInit(IMU_Driver *driver)
{
    bool found = false;

    // Test The connection
    DBGLN("Detecting LSM6DXX");

    for (int8_t i = 0; i < 5; i++)
    {
        if (lsm6dxxDetect(driver))
        {
            found = true;
            break;
        }
        vTaskDelay(50 * portTICK_PERIOD_MS);
    }

    if (!found)
    {
        DBGLN("LSM6DXX not found!");
        return false;
    }

    DBGLN("LSM6DXX found!!");

    driver->gyroSampleRate = 833;
    driver->period_us = (1000000 / driver->gyroSampleRate);

    accScaleCode = LSM6DXX_VAL_CTRL1_XL_16G; // Acceleation 16G
    driver->accScaleG = 16 / 32768.0;        //   multiply adc by this to get Gs
    driver->acc1G_adc = 32768 / 16;          //   1G in adc values

    gyroScaleCode = LSM6DXX_VAL_CTRL2_G_2000DPS;
    driver->gyroScaleDeg = 2000.0 / 32768.0;              //   multiply adc by this to get deg°/s
    driver->gyroScaleRad = radians(driver->gyroScaleDeg); //   multiply adc by this to get rad°/s

    return true;
}

const char *IMU_LSM6DXX_SPI::GetMPUName()
{
    switch (lsm6dID)
    {
    case LSM6DSO_CHIP_ID:
        return "LSM6DSO";
    case LSM6DSL_CHIP_ID:
        return "LSM6DSL";
    }
    return "LSM6Dxx";
}

bool IMU_LSM6DXX_SPI::initialize()
{
    // Initialize CS
    cs_pin = GPIO_PIN_GYRO_NSS;
    int_pin = GPIO_PIN_GYRO_INT;
    DBGLN("LSM6DXX(SPI) NSS Pin=%d, INT Pin=%d", cs_pin, int_pin);

    pinMode(cs_pin, OUTPUT);
    digitalWrite(cs_pin, HIGH);

    _spiSettings = SPISettings(10000000, MSBFIRST, SPI_MODE3);

    if (int_pin != 0)
    {
        setupInterrupt(int_pin);
    }

    if (lsm6dxxInit(this))
    {
        lsm6dxxConfig(this);
        setInterruptReceived(true); // True to force read at lest the first time
        return true;
    }
    return false;
}

void IMU_LSM6DXX_SPI::start()
{
    DBGLN("LSM6DXX(SPI) Start");
    setInterruptReceived(true); // Forced to read at least the first time
}

bool IMU_LSM6DXX_SPI::isDataReady()
{
    static unsigned long last_ready = micros();

    unsigned long now = micros();

    if (int_pin != 0)
    { // Using Interrupt pin??
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
        // Non-Interrupt, read the status registers
        return lsm6dxxAccGyroDataReady(this);
    }
}

bool IMU_LSM6DXX_SPI::rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz)
{
    setInterruptReceived(false);
    return lsm6dxxAccGyroRead(this, ax, ay, az, gx, gy, gz);
}

const char *IMU_LSM6DXX_I2C::GetMPUName()
{
    switch (lsm6dID)
    {
    case LSM6DSO_CHIP_ID:
        return "LSM6DSO";
    case LSM6DSL_CHIP_ID:
        return "LSM6DSL";
    }
    return "LSM6Dxx";
}

bool IMU_LSM6DXX_I2C::initialize()
{
    m_address = LSM6DSV16X_ADDRESS_LOW;
    if (lsm6dxxInit(this))
    {
        lsm6dxxConfig(this);
        return true;
    }
    return false;
}

void IMU_LSM6DXX_I2C::start()
{
    DBGLN("LSM6DXX(I2C) Start");
    setInterruptReceived(true);
}

bool IMU_LSM6DXX_I2C::isDataReady()
{
    return lsm6dxxAccGyroDataReady(this);
}

bool IMU_LSM6DXX_I2C::rawRead(int16_t *ax, int16_t *ay, int16_t *az, int16_t *gx, int16_t *gy, int16_t *gz)
{
    setInterruptReceived(false);
    return lsm6dxxAccGyroRead(this, ax, ay, az, gx, gy, gz);
}

#endif
