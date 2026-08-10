#if defined(GYRO_SUPPORT) && defined(PLATFORM_ESP32)

#include "devGyro.h"
#include "gyro.h"
#include "logging.h"

void gyroQuickModelSetup(int wingType, int tailType)
{
    gyro.pause();
    gyroConfig->SetDefaults(false);

    gyroConfig->SetGyroChannel(8, FN_GYRO_MODE, false, false); // Mode
    gyroConfig->SetGyroChannel(9, FN_GYRO_GAIN, false, false); // Gain

    switch (wingType)
    {
    case 0:
        break;                                                   // Empty
    case 2:                                                      // 2-Aileron
        gyroConfig->SetGyroChannel(5, FN_AILERON, false, false); // Ail2
        // continue to normal
    case 1:                                                     // Normal
        gyroConfig->SetGyroChannel(0, FN_AILERON, true, false); // Ail MAster
        break;
    case 3:                                                       // DELTA
        gyroConfig->SetGyroChannel(0, FN_ELEVON, true, false);    // Ail, MASTER
        gyroConfig->SetGyroChannel(1, FN_ELEVON_R, false, false); // Ele
        break;
    }

    switch (tailType)
    {
    case 0:
        break;                                                   // Empty
    case 1:                                                      // Normal
        gyroConfig->SetGyroChannel(1, FN_ELEVATOR, true, false); // Ele Master
        gyroConfig->SetGyroChannel(3, FN_RUDDER, true, false);   // Rud
        break;
    case 2:                                                      // Vtail
        gyroConfig->SetGyroChannel(1, FN_VTAIL, true, false);    // Ele, MASTER
        gyroConfig->SetGyroChannel(3, FN_VTAIL_R, false, false); // Rud
        break;
    case 3:                                                       // Taileron
        gyroConfig->SetGyroChannel(0, FN_ELEVON, false, false);   // Ail
        gyroConfig->SetGyroChannel(1, FN_ELEVON_R, false, false); // Ele
        gyroConfig->SetGyroChannel(3, FN_RUDDER, true, false);    // Rud, MASTER
        break;
    case 4:                                                    // Rudder only
        gyroConfig->SetGyroChannel(3, FN_RUDDER, true, false); // Rud, MASTER
        break;
    }

    gyroConfig->Commit();
    gyro.reload();
}

bool gyroIsVisible(gyro_mode_t fm, gyro_ui_vibility_t category)
{
    bool ret = false;

    switch (category)
    {
    case GYRO_UI_TRIMS:
        ret = (fm == GYRO_MODE_LEVEL || fm == GYRO_MODE_LAUNCH);
        break;
    case GYRO_UI_GAINS:
        ret = true;
        break;
    case GYRO_UI_USE_RATE:
        ret = (fm != GYRO_MODE_RATE);
        break;
    case GYRO_UI_STICK_PRIORITY:
        ret = (fm == GYRO_MODE_RATE);
        break;
    case GYRO_UI_MAX_ANGLE:
        ret = (fm == GYRO_MODE_ENVELOPE || fm == GYRO_MODE_LEVEL);
        break;
    }

    return ret;
}

static void gyroUpgrade2_to_3()
{
    for (int fm = GYRO_MODE_RATE; fm <= GYRO_MODE_LAST_ACTIVE; fm++)
    { // Skip Gyro OFF, 1 based
        rx_config_gyro_fmode_t tmp;

        memcpy(&tmp, gyroConfig->GetGyroFMode((gyro_mode_t)fm), sizeof(rx_config_gyro_fmode_t));

        // For Auto-Level/Launch
        tmp.val.trimPitch = (fm == GYRO_MODE_LAUNCH) ? 10 : 0;
        tmp.val.trimRoll = 0;

        tmp.val.useRate = 1;

        gyroConfig->SetGyroFModeRaw((gyro_mode_t)fm, tmp.raw);
    }
}

void gyroUpgrade(uint8_t version)
{
    if (version == 2)
    {
        gyroUpgrade2_to_3();
        version = 3;
    }

    if (version == 3)
    {
        const rx_config_gyro_PID_t *madgwickPI = gyroConfig->GetGyroPID(GYRO_PID_GROUP_MADGWICK, GYRO_AXIS_ROLL);
        if (madgwickPI->val.p == 0)
        { // Madgwick not configured
            DBGLN("Setting Defaults for Madgwick-AHRS");
            // MADGWICK PI  (P=2.0, I=0.0)
            gyroConfig->SetGyroPIDRate(GYRO_PID_GROUP_MADGWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_P, 20);
            gyroConfig->SetGyroPIDRate(GYRO_PID_GROUP_MADGWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_I, 00);
            gyroConfig->SetGyroPIDRate(GYRO_PID_GROUP_MADGWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_D, 00);
        }
        // gyroUpgrade3_to_4();
    }
}

/* encode/decode negative numbers in 6 bits*/
int8_t gyro_trim_decode(int8_t n)
{
    if (n > 31)
        return -(n - 31);
    else
        return n;
}

int8_t gyro_trim_encode(int8_t n)
{
    if (n < 0)
        return 31 + (-n);
    else
        return n;
}

#endif
