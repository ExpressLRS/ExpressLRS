#include "config.h"
#include "gyro.h"
#include "logging.h"

#if defined(GYRO_SUPPORT)

static char *wingTypeName[]={"Empty","Normal","2-Ail","Delta"};
static char *tailTypeName[]={"Empty","Normal","V-Tail","Taileron","Rud-Only"};

void gyroSetConfigDefaults() {
    config.SetGyroConfigVersion(GYRO_CONFIG_VERSION);
    config.SetGyroEnabled(false);
    config.SetGyroOrientation(6,6); // 6=No orientation Set
    config.SetGyroGainFactor(GYRO_GAIN_FACTOR_1X);

    // Configure Limits
    for (unsigned int ch=0; ch < PWM_MAX_CHANNELS; ch++) {
         config.SetPwmChannelLimits(ch, GYRO_US_MIN, GYRO_US_MAX, GYRO_US_MID);
    }

    // Configure PIDS
    for (int g=0;g<=GYRO_PID_GROUP_LAST_ACTIVE;g++) {
        auto group = (gyro_pidgroup_t) g;
        for (int i=0;i<3;i++) {
            auto axis = (gyro_axis_t) i;
            config.SetGyroPIDRate(group, axis,GYRO_RATE_VARIABLE_P, 35);
            config.SetGyroPIDRate(group, axis,GYRO_RATE_VARIABLE_I, 0);
            config.SetGyroPIDRate(group, axis,GYRO_RATE_VARIABLE_D, 10);
        }
    }

    // MADWICK PI
    config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_P, 20);
    config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_I, 00);
    config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_D, 00);
 

    // Configure channel Functions
    for (int ch=0;ch<CRSF_NUM_CHANNELS;ch++) {
        config.SetGyroChannel(0, FN_NONE, false, false);
    }

    // Configure Gyro Switch
    for (int p=0;p<5;p++) {
       config.SetGyroModePos(p, GYRO_MODE_OFF);
    }
    config.SetGyroModePos(0, GYRO_MODE_OFF);
    config.SetGyroModePos(1, GYRO_MODE_OFF);
    config.SetGyroModePos(2, GYRO_MODE_RATE);
    config.SetGyroModePos(3, GYRO_MODE_OFF);
    config.SetGyroModePos(4, GYRO_MODE_LEVEL);

    for (int fm=GYRO_MODE_RATE; fm <= GYRO_MODE_LAST_ACTIVE; fm++) { // Skip Gyro OFF, 1 based 
        rx_config_gyro_fmode_t tmp;
        tmp.raw = 0;

        tmp.val.useRate = 1;

        // Only Rate
        tmp.val.stickPri = STICK_PRIORITY_100;

        // For Auto-Level/Envelope
        tmp.val.maxAnglePitch = 40;  
        tmp.val.maxAngleRoll  = 70;
        
        // For Auto-Level/Launch
        tmp.val.trimPitch   = (fm == GYRO_MODE_LAUNCH)?10:0;
        tmp.val.trimRoll    = 0;

        // Gains for everybody
        tmp.val.gainRoll  = (fm == GYRO_MODE_RATE)?30:35;
        tmp.val.gainPitch = (fm == GYRO_MODE_RATE)?40:35;
        tmp.val.gainYaw   = (fm == GYRO_MODE_RATE)?50:35;

        config.SetGyroFModeRaw((gyro_mode_t) fm, tmp.raw);
    }
}

void gyroQuickModelSetup(int wingType, int tailType) {
  gyro.pause();
  gyroSetConfigDefaults();
  
  config.SetGyroChannel(8, FN_GYRO_MODE, false, false); // Mode
  config.SetGyroChannel(9, FN_GYRO_GAIN, false, false); // Gain

  switch (wingType) {
    case 0: break;  // Empty
    case 2: // 2-Aileron
        config.SetGyroChannel(5, FN_AILERON, false, false); // Ail2
        // continue to normal
    case 1: // Normal
        config.SetGyroChannel(0, FN_AILERON, true, false); // Ail MAster
        break;
    case 3: // DELTA
        config.SetGyroChannel(0, FN_ELEVON, true, false); // Ail, MASTER
        config.SetGyroChannel(1, FN_ELEVON_R, false, false); // Ele
        break;
  }

  switch (tailType) {
    case 0: break;  // Empty
    case 1: // Normal
        config.SetGyroChannel(1, FN_ELEVATOR, true, false); // Ele Master
        config.SetGyroChannel(3, FN_RUDDER, true, false); // Rud
        break;
    case 2: //Vtail
        config.SetGyroChannel(1, FN_VTAIL, true, false); // Ele, MASTER
        config.SetGyroChannel(3, FN_VTAIL_R, false, false);   // Rud
        break;
    case 3: // Taileron
        config.SetGyroChannel(0, FN_ELEVON, false, false); // Ail
        config.SetGyroChannel(1, FN_ELEVON_R, false, false); // Ele
        config.SetGyroChannel(3, FN_RUDDER, true, false); // Rud, MASTER
        break;
    case 4: //Rudder only
        config.SetGyroChannel(3, FN_RUDDER, true, false); // Rud, MASTER
        break;
  }

  config.Commit();
  gyro.reload();
}

bool gyroIsVisible(gyro_mode_t fm, gyro_ui_vibility_t category) {
    bool ret = false;

    switch (category) {
        case GYRO_UI_TRIMS: ret = (fm == GYRO_MODE_LEVEL || fm == GYRO_MODE_LAUNCH);break;
        case GYRO_UI_GAINS: ret = true; break;
        case GYRO_UI_USE_RATE: ret = (fm != GYRO_MODE_RATE); break;
        case GYRO_UI_STICK_PRIORITY: ret = (fm==GYRO_MODE_RATE); break;
        case GYRO_UI_MAX_ANGLE: ret = (fm==GYRO_MODE_ENVELOPE || fm==GYRO_MODE_LEVEL); break;
    }

    return ret;
}

 static void gyroUpgrade2_to_3()
 {
      for (int fm=GYRO_MODE_RATE; fm <= GYRO_MODE_LAST_ACTIVE; fm++) { // Skip Gyro OFF, 1 based 
        rx_config_gyro_fmode_t tmp;
        
        memcpy(&tmp,config.GetGyroFMode((gyro_mode_t) fm), sizeof(rx_config_gyro_fmode_t));
        
        // For Auto-Level/Launch
        tmp.val.trimPitch   = (fm == GYRO_MODE_LAUNCH)?10:0;
        tmp.val.trimRoll    = 0;

        tmp.val.useRate = 1;

        config.SetGyroFModeRaw((gyro_mode_t) fm, tmp.raw);
    }
 }

  void gyroUpgrade(uint8_t version) {
    if (version == 2) {
      gyroUpgrade2_to_3();
      version = 3;
    }

    if (version == 3) {
        const rx_config_gyro_PID_t *madwickPI = config.GetGyroPID(GYRO_PID_GROUP_MADWICK,GYRO_AXIS_ROLL);
        if (madwickPI->p==0) {  // Madwick not configured
            DBGLN("Setting Defaults for Madwick-AHRS");
            // MADWICK PI  (P=2.0, I=0.0)
            config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_P, 20);
            config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_I, 00);
            config.SetGyroPIDRate(GYRO_PID_GROUP_MADWICK, GYRO_AXIS_ROLL, GYRO_RATE_VARIABLE_D, 00);
        }
      //gyroUpgrade3_to_4();
    }
  }

/* encode/decode negative numbers in 6 bits*/
int8_t gyro_trim_decode(int8_t n) 
{
    if (n > 31) return -(n - 31); else return n; 
}

int8_t gyro_trim_encode(int8_t n) 
{
    if (n < 0) return 31 + (-n); else return n;
}


#endif
