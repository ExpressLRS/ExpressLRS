#include "targets.h"
#if !defined(UNIT_TEST)
#include "RXEndpoint.h"
#include "FHSS.h"
#include "POWERMGNT.h"
#include "config.h"
#include "deferred.h"
#include "devServoOutput.h"
#include "helpers.h"
#include "rxtx_intf.h"
#include "logging.h"

#if defined(PLATFORM_ESP32)
#include "devGyro.h"
#include "gyro.h"
extern void gyroQuickModelSetup(int wingType, int tailType);
#endif

#define RX_HAS_SERIAL1 (GPIO_PIN_SERIAL1_TX != UNDEF_PIN || OPT_HAS_SERVO_OUTPUT)

extern void reconfigureSerial();
#if defined(PLATFORM_ESP32)
extern void reconfigureSerial1();
#endif
extern bool BindingModeRequest;

extern RXEndpoint crsfReceiver;

#if defined(Regulatory_Domain_EU_CE_2400)
#if defined(RADIO_LR1121)
char strPowerLevels[] = "10/10;25/25;25/50;25/100;25/250;25/500;25/1000;25/2000;MatchTX ";
#else
char strPowerLevels[] = "10;25;50;100;250;500;1000;2000;MatchTX ";
#endif
#else
char strPowerLevels[] = "10;25;50;100;250;500;1000;2000;MatchTX ";
#endif
static char modelString[] = "000";
static char pwmModes[] = "50Hz;60Hz;100Hz;160Hz;333Hz;400Hz;10kHzDuty;On/Off;DShot;DShot 3D;Serial RX;Serial TX;I2C SCL;I2C SDA;Serial2 RX;Serial2 TX";

#if defined(PLATFORM_ESP32)
static char gyroOffOn[] = "Off;On"; // Off-ON
//  needs to match gyro_status_t
static const char *gyroStatus[] = {"Off","IMU Not Detected","Need RX-Orientation","Need Stick Calibration","Running"};

// Orientation Names in MPU
extern const char* mpuOrientationNames[];

// Must match mixer.h: gyro_output_channel_function_t
static char gyroOutputChannelModes[] = "None;Aileron;Elevator;Rudder;Elevon;Elevon_Inv;VTail;VTail_Inv;Mode;Gain";
// Must match gyro.h gyro_mode_t
static const char switch_gyroModes[] = "Off;Rate;Envelope;Auto-Level;Launch;Hover";
static const char fmodes[] = "Rate;Envelope;Auto-Level;Launch;Hover";

// Must match gyro_pidgroup_t
static const char gyroPidGroup[] = "Rate (v/100);Level(v/100);AHRS (v/10)";
// Must match gyro_axis_t
static const char gyroAxis[] = "Roll;Pitch;Yaw";

static char gyroStatusStr[30]; // Display Status + Version
static char gyroIMUStatusStr[30]; // Display Gyro and Errors
static char gyroIMUErrorStr[30];

static const char gyroRxOrientationsHR[] = 
    {"UART Up(X+);UART Dn(X-);Pins Up(Y+);Pins Dn(Y-);Lbl Up(Z+);Lbl Dn(Z-);WRONG;WRONG"};

static const char gyroRxOrientationsRM[] = 
    {"Pins Up(X+);Pins Dn(X-);V-Lbl Up(Y+);V-Lbl Dn(Y-);Lbl Up(Z+);Lbl Dn(Z-);WRONG;WRONG"};

static const char *getRxOrientationOptions() 
{
  return OPT_HAS_GYRO_MPU6050?gyroRxOrientationsHR:gyroRxOrientationsRM;
}


#endif

static selectionParameter luaSerialProtocol = {
    {"Protocol", CRSF_TEXT_SELECTION},
    0, // value
    "CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;MAVLink;DisplayPort;GPS",
    STR_EMPTYSPACE
};

#if defined(PLATFORM_ESP32)
static selectionParameter luaSerial1Protocol = {
    {"Protocol2", CRSF_TEXT_SELECTION},
    0, // value
    "Off;CRSF;Inverted CRSF;SBUS;Inverted SBUS;SUMD;DJI RS Pro;HoTT Telemetry;Tramp;SmartAudio;DisplayPort;GPS",
    STR_EMPTYSPACE
};
#endif

static selectionParameter luaSBUSFailsafeMode = {
    {"SBUS failsafe", CRSF_TEXT_SELECTION},
    0, // value
    "No Pulses;Last Pos",
    STR_EMPTYSPACE
};

static int8Parameter luaTargetSysId = {
    {"Target SysID", CRSF_UINT8},
    {
        {
            (uint8_t)1,       // value - default to 1
            (uint8_t)1,       // min
            (uint8_t)255,     // max
        }
    },
    STR_EMPTYSPACE
};

static int8Parameter luaSourceSysId = {
    {"Source SysID", CRSF_UINT8},
    {
        {
            (uint8_t)255,       // value - default to 255
            (uint8_t)1,         // min
            (uint8_t)255,       // max
        }
    },
    STR_EMPTYSPACE
};

static selectionParameter luaTlmPower = {
    {"Tlm Power", CRSF_TEXT_SELECTION},
    0, // value
    strPowerLevels,
    "mW"
};

static selectionParameter luaAntennaMode = {
    {"Ant. Mode", CRSF_TEXT_SELECTION},
    0, // value
    "Antenna A;Antenna B;Diversity",
    STR_EMPTYSPACE
};

static selectionParameter luaAntennaGroup = {
    {"Ant. Group", CRSF_TEXT_SELECTION},
    0, // value
    "External;Builtin",
    STR_EMPTYSPACE
};

static folderParameter luaTeamraceFolder = {
    {"Team Race", CRSF_FOLDER},
};

static selectionParameter luaTeamraceChannel = {
    {"Channel", CRSF_TEXT_SELECTION},
    0, // value
    "AUX2;AUX3;AUX4;AUX5;AUX6;AUX7;AUX8;AUX9;AUX10;AUX11;AUX12",
    STR_EMPTYSPACE
};

static selectionParameter luaTeamracePosition = {
    {"Position", CRSF_TEXT_SELECTION},
    0, // value
    "Disabled;1/Low;2;3;Mid;4;5;6/High",
    STR_EMPTYSPACE
};

//----------------------------Info-----------------------------------

static stringParameter luaModelNumber = {
    {"Model Id", CRSF_INFO},
    modelString
};

static stringParameter luaELRSversion = {
    {version_domain, CRSF_INFO},
    commit
};

//----------------------------Info-----------------------------------

//---------------------------- WiFi -----------------------------

// --------------------------- Gyro Setup ---------------------------------

#if defined(PLATFORM_ESP32)
static const char *wingTypeName[] {"Empty","Normal","2-Ail","Delta"};
static auto wingTypeStr = "Empty;Normal;2-Ail;Delta";

static const char *tailTypeName[] {"Empty","Normal","V-Tail","Taileron","Rud-Only"};
static auto tailTypeStr = "Empty;Normal;V-Tail;Taileron;Rud-only";

static selectionParameter luaGyroEnabled = {
    {"Enable Gyro", CRSF_TEXT_SELECTION},
    0, // value
    gyroOffOn,
    STR_EMPTYSPACE
};

static stringParameter luaGyroStatus = {
    {"Status", CRSF_INFO},
    "" // value
};

static stringParameter luaGyroIMUStatus = {
    {"IMUStatus", CRSF_INFO},
    "" // value
};

static stringParameter luaGyro_Warning = {
    {"WARNING: Auto-Level as Panic is Experimental", CRSF_INFO},
    STR_EMPTYSPACE
};

static selectionParameter luaGyroGainFactor = {
    {"Rate Gain Mult", CRSF_TEXT_SELECTION},
    0, // value
    "0.5x;1x;1.5x;2x",
    STR_EMPTYSPACE
};

static struct commandParameter luaGyroMainRefresh = {
    {"Refresh Page", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static folderParameter luaGyroMainFolder = {
    {"Gyro", CRSF_FOLDER},
};

static folderParameter luaGyroModelFolder = {
    {"Model Setup", CRSF_FOLDER},
};

static folderParameter luaGyroModesFolder = {
    {"F-Mode Switch", CRSF_FOLDER},
};

static folderParameter luaGyroPIDFolder = {
    {"PIDs (Advanced)", CRSF_FOLDER},
};

static folderParameter luaGyroOutputFolder = {
    {"Ch Functions", CRSF_FOLDER},
};

static folderParameter luaGyroSettingsFolder = {
    {"Gyro Settings", CRSF_FOLDER},
};

static folderParameter luaGyroFModeFolder = {
    {"FMode Settings", CRSF_FOLDER},
};

static folderParameter luaGyroQuickSetupFolder = {
    {"Quick Setup", CRSF_FOLDER},
};

static folderParameter luaGyroCalibrationFolder = {
    {"Calibration", CRSF_FOLDER},
};

static folderParameter luaGyroRxOrientationFolder = {
    {"RX Orientation", CRSF_FOLDER},
};

static selectionParameter luaGyroModePos1 = {
    {"Pos 1 (-100%)", CRSF_TEXT_SELECTION},
    0, // value
    switch_gyroModes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroModePos2 = {
    {"Pos 2 (-50%)", CRSF_TEXT_SELECTION},
    0, // value
    switch_gyroModes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroModePos3 = {
    {"Pos 3 (0%)", CRSF_TEXT_SELECTION},
    0, // value
    switch_gyroModes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroModePos4 = {
    {"Pos 4 (+50%)", CRSF_TEXT_SELECTION},
    0, // value
    switch_gyroModes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroModePos5 = {
    {"Pos 5 (+100%)", CRSF_TEXT_SELECTION},
    0, // value
    switch_gyroModes,
    STR_EMPTYSPACE
};

//------------  Output Channel Settings -------------
static int8Parameter luaGyroOutputCh_Select = {
    {"Output Ch ->", CRSF_UINT8},
    {
        {
            (uint8_t)1,       // value, not zero-based
            1,                // min
            PWM_MAX_CHANNELS, // max
        }
    },
    STR_EMPTYSPACE
};

static selectionParameter luaGyroOutputCh_Mode = {
    {"Function", CRSF_TEXT_SELECTION},
    0, // value
    gyroOutputChannelModes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroOutputCh_Master = {
    {"Ch Master (Stick)", CRSF_TEXT_SELECTION},
    0, // value
    gyroOffOn,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroOutputCh_Inverted = {
    {"Dir Reverse", CRSF_TEXT_SELECTION},
    0, // value
    gyroOffOn,
    STR_EMPTYSPACE
};


void RXEndpoint::luaparamGyroOutputCh_Select(propertiesCommon *item, uint8_t arg)
{
    setUint8Value(&luaGyroOutputCh_Select, arg);
    // Reload Dependent Values
    const rx_config_gyro_channel_t *gyroChOut = gyroConfig->GetGyroChannel(arg - 1);
    setTextSelectionValue(&luaGyroOutputCh_Mode, gyroChOut->val.output_mode);
    setTextSelectionValue(&luaGyroOutputCh_Master, gyroChOut->val.master);
    setTextSelectionValue(&luaGyroOutputCh_Inverted, gyroChOut->val.inverted);
  
    // Don't display Master or Inverted options for Mode or Gain functions.
    LUA_FIELD_VISIBLE(luaGyroOutputCh_Master, gyroChOut->val.output_mode != FN_GYRO_MODE && gyroChOut->val.output_mode != FN_GYRO_GAIN);
    LUA_FIELD_VISIBLE(luaGyroOutputCh_Inverted, gyroChOut->val.output_mode != FN_GYRO_MODE && gyroChOut->val.output_mode != FN_GYRO_GAIN);
    // Filter out Mode/Gain depending if they are used by another channel.
    bool includeMode = true;
    bool includeGain = true;
    for (int i=0 ; i<GYRO_MAX_CHANNELS ; i++)
    {
        const rx_config_gyro_channel_t *gyroCh = gyroConfig->GetGyroChannel(i);
        if (gyroCh->val.output_mode == FN_GYRO_MODE && i != arg - 1) includeMode = false;
        if (gyroCh->val.output_mode == FN_GYRO_GAIN && i != arg - 1) includeGain = false;
    }
    char *pos = strstr(gyroOutputChannelModes, "VTail_Inv;");
    pos += 10;
    *pos = 0;
    if (includeMode) strcat(pos, "Mode;");
    else strcat(pos, ";");
    if (includeGain) strcat(pos, "Gain");
}

static void luaparamGyroOutputCh_Mode(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaGyroOutputCh_Select.properties.u.value - 1;
    rx_config_gyro_channel_t newCh;
    newCh.raw = gyroConfig->GetGyroChannel(ch)->raw;
    newCh.val.output_mode = arg;
    if (arg==FN_NONE) { // Clear other flags
      newCh.val.master = false;
      newCh.val.inverted = false;
    }
    gyroConfig->SetGyroChannelRaw(ch, newCh.raw);
    gyro.reload();
    // Don't display Master or Inverted options for Mode or Gain functions.
    LUA_FIELD_VISIBLE(luaGyroOutputCh_Master, arg != FN_GYRO_MODE && arg != FN_GYRO_GAIN);
    LUA_FIELD_VISIBLE(luaGyroOutputCh_Inverted, arg != FN_GYRO_MODE && arg != FN_GYRO_GAIN);
}

static void luaparamGyroOutputCh_Master(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaGyroOutputCh_Select.properties.u.value - 1;
    rx_config_gyro_channel_t newCh;
    newCh.raw = gyroConfig->GetGyroChannel(ch)->raw;
    newCh.val.master = arg;
    gyroConfig->SetGyroChannelRaw(ch, newCh.raw);
    gyro.reload();
}

static void luaparamGyroOutputCh_Inverted(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaGyroOutputCh_Select.properties.u.value - 1;
    rx_config_gyro_channel_t newCh;
    newCh.raw = gyroConfig->GetGyroChannel(ch)->raw;
    newCh.val.inverted = arg;
    gyroConfig->SetGyroChannelRaw(ch, newCh.raw);
    gyro.reload();
}

//------------  Gyro Gains Settings -------------
static selectionParameter luaGyroPID_Select_Group = {
    {"PID Group ->", CRSF_TEXT_SELECTION},
    0, // value
    gyroPidGroup,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroPID_Select_Axis = {
    {"PID Axis ->", CRSF_TEXT_SELECTION},
    0, // value
    gyroAxis,
    STR_EMPTYSPACE
};

static int8Parameter luaGyroPID_RateP = {
    {"P Rate", CRSF_UINT8},
    {
        {
            (uint8_t)1, // value
            0,          // min
            100         // max
        }
    },
    STR_EMPTYSPACE};

static int8Parameter luaGyroPID_RateI = {
    {"I Rate", CRSF_UINT8},
    {
        {
            (uint8_t)1, // value
            0,          // min
            100         // max
        }
    },
    STR_EMPTYSPACE};

static int8Parameter luaGyroPID_RateD = {
    {"D Rate/LPF HZ", CRSF_UINT8},
    {
        {
            (uint8_t)1, // value
            0,          // min
            100         // max
        }
    },
    STR_EMPTYSPACE};

static void luaparamGyroPID_RateP(propertiesCommon *item, uint8_t arg)
{
    const gyro_pidgroup_t group = (gyro_pidgroup_t) luaGyroPID_Select_Group.value;
    const gyro_axis_t axis = (gyro_axis_t) luaGyroPID_Select_Axis.value;
    gyroConfig->SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_P, arg);
    gyro.reload();
}

static void luaparamGyroPID_RateI(propertiesCommon *item, uint8_t arg)
{
    const gyro_pidgroup_t group = (gyro_pidgroup_t) luaGyroPID_Select_Group.value;
    const gyro_axis_t axis = (gyro_axis_t) luaGyroPID_Select_Axis.value;
    gyroConfig->SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_I, arg);
    gyro.reload();
}

static void luaparamGyroPID_RateD(propertiesCommon *item, uint8_t arg)
{
    const gyro_pidgroup_t group = (gyro_pidgroup_t) luaGyroPID_Select_Group.value;
    const gyro_axis_t axis = (gyro_axis_t) luaGyroPID_Select_Axis.value;
    gyroConfig->SetGyroPIDRate(group, axis, GYRO_RATE_VARIABLE_D, arg);
    gyro.reload();
}

//------------  Gyro RX Orientation Info -------------
static selectionParameter luaGyroOrientationH = {
    {"Hor (Level)", CRSF_TEXT_SELECTION},
    6, // WRONG orintation
    getRxOrientationOptions(),
    STR_EMPTYSPACE
};

static selectionParameter luaGyroOrientationV = {
    {"Vert (Nose DOWN)", CRSF_TEXT_SELECTION},
    6, // WRONG orintation
    getRxOrientationOptions(),
    STR_EMPTYSPACE};

//---------  Reset Commands ---------------------

static selectionParameter luaGyroQuickSetup_wingType_Select = {
    {"Wing Type ->", CRSF_TEXT_SELECTION},
    0, // value
    wingTypeStr,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroQuickSetup_tailType_Select = {
    {"Tail Type  ->", CRSF_TEXT_SELECTION},
    0, // value
    tailTypeStr,
    STR_EMPTYSPACE
};

static struct commandParameter luaGyroQuickPreset = {
    {"Execute", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

void RXEndpoint::luaparamGyroQuickPreset(propertiesCommon *item, uint8_t arg)
{
    static char temp[50];
    static int8_t step = 0;

    commandStep_e newStep;
    const char *msg;
    if (arg == lcsClick)
    {
        step = 0;
        newStep = lcsAskConfirm;
        if (luaGyroQuickSetup_wingType_Select.value == 0)
        {
            msg = "Reset to EMPTY model ?";
        }
        else
        {
            sprintf(temp, "Reset Model to W=%s T=%s?", wingTypeName[luaGyroQuickSetup_wingType_Select.value], tailTypeName[luaGyroQuickSetup_tailType_Select.value]);
            msg = temp;
        }
    }
    else if (arg == lcsConfirmed)
    {
        // This is generally not seen by the user, since we'll disconnect to commit config
        // and the handset will send another lcdQuery that will overwrite it with idle
        newStep = lcsExecuting;
        if (step == 0)
        {
            msg = "Creating Model";
            gyroQuickModelSetup(luaGyroQuickSetup_wingType_Select.value, luaGyroQuickSetup_tailType_Select.value);
            step++;
        }
        else
        {
            // Done confirmation
            newStep = lcsIdle;
            msg = STR_EMPTYSPACE;
        }
    }
    else if (arg == lcsQuery)
    {
        if (step == 1)
        {
            // Step 1: Done
            newStep = lcsAskConfirm;
            msg = "Done: Restart LUA script ";
        }
        else
        {
            newStep = lcsIdle;
            msg = STR_EMPTYSPACE;
        }
    }
    else
    {
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }

    sendCommandResponse((commandParameter *)item, newStep, msg);
}

//-----------  Gyro Calibration ------------------------
static struct commandParameter luaGyroCalibration = {
    {"Gyro Level Calibration", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

void RXEndpoint::luaparamGyroCalibration(propertiesCommon *item, uint8_t arg)
{
    commandStep_e newStep;
    const char *msg;
    if (arg == lcsClick)
    {
        newStep = lcsAskConfirm;
        msg = "Plane/RX Level??";
        gyro.pause();
    }
    else if (arg == lcsConfirmed)
    {
        // This is generally not seen by the user, since we'll disconnect to commit config
        // and the handset will send another lcdQuery that will overwrite it with idle
        newStep = lcsExecuting;
        msg = "Level Cal";
        sendCommandResponse((commandParameter *)item, newStep, msg);
        gyro.calibrate();
        gyro.reload();
        return;
    }
    else
    {
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
        gyro.reload();
    }

    sendCommandResponse((commandParameter *)item, newStep, msg);
}

//--------------------- RX Orientation Calibration -----------------
static struct commandParameter luaGyroAutoOrientation = {
    {"Detect Orientation", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

void RXEndpoint::luaparamGyroOrientationCal(propertiesCommon *item, uint8_t arg)
{
    static uint8_t calStep = 0; // Global

    commandStep_e newStep;
    const char *msg = STR_EMPTYSPACE;

    // DBGLN("Calibration Workflow BEGIN: command=[%d],calStep=[%d]",arg,calStep);
    if (arg == lcsClick)
    {
        // Step 1: Horizontal
        calStep = 0;
        DBGLN("Calibrating Gyro: Gyro Ready=%s", gyro.initialized ? "True" : "False");
        newStep = lcsAskConfirm;
        msg = "Plane/RX Level?";
        gyro.pause(); // Suspend Gyro
    }
    else if (arg == lcsConfirmed)
    {
        // This is generally not seen by the user, since we'll disconnect to commit config
        // and the handset will send another lcdQuery that will overwrite it with idle
        newStep = lcsExecuting;
        if (calStep == 0)
        {
            msg = "Level Cal";
            sendCommandResponse((commandParameter *)item, newStep, msg);
            calStep++;
            gyro.ahrs->OrientationHorizontalExecute();
            return;
        }
        else if (calStep == 1)
        {
            msg = "Vertical Det";
            sendCommandResponse((commandParameter *)item, newStep, msg);
            calStep++;
            gyro.ahrs->OrientationVerticalExecute();
            gyro.reload(); // This will resume Gyro
            return;
        }
        else if (calStep == 2)
        {
            // Calibration Done
            newStep = lcsIdle;
            msg = STR_EMPTYSPACE;
        }
    }
    else if (arg == lcsQuery)
    {
        if (calStep == 1)
        {
            // Step 2: Vertical
            newStep = lcsAskConfirm;
            msg = "Plane Nose DOWN?";
        }
        else if (calStep == 2)
        {
            // Step 3: Done
            newStep = lcsAskConfirm;
            msg = "Calibration Done";
        }
        else
        {
            msg = STR_EMPTYSPACE;
            newStep = lcsIdle;
        }
    }
    else if (arg == lcsCancel)
    {
        gyro.reload(); // Reactivate Gyro if cancelation
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }
    else // idle
    {
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }

    // DBGLN("Calibrating Workflow RETURN: newStep=[%d],msg=[%s], calStep=[%d]",newStep,msg,calStep);
    sendCommandResponse((commandParameter *)item, newStep, msg);
}

//--------------------- RX Stick Limit/subtrim Calibration -----------------
static struct commandParameter luaGyroStickCal = {
    {"Stick Calibration", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

void RXEndpoint::luaparamGyroStickCal(propertiesCommon *item, uint8_t arg)
{
    static uint8_t calStep = 0; // Global

    commandStep_e newStep;
    const char *msg = STR_EMPTYSPACE;

    // DBGLN("Calibration Workflow BEGIN: command=[%d],calStep=[%d]",arg,calStep);
    if (arg == lcsClick)
    {
        // Step 1: Horizontal
        calStep = 0;
        DBGLN("Gyro(): Calibrating Sticks");
        newStep = lcsAskConfirm;
        msg = "Sticks Centered?";
    }
    else if (arg == lcsConfirmed)
    {
        // This is generally not seen by the user, since we'll disconnect to commit config
        // and the handset will send another lcdQuery that will overwrite it with idle
        newStep = lcsExecuting;
        if (calStep == 0)
        {
            msg = "Stick Center";
            calStep++;
            gyro.StickCenterCalibration();
        }
        else if (calStep == 1)
        {
            msg = "Stick Range";
            calStep++;
        }
        else if (calStep == 2)
        {
            // Calibration Done
            newStep = lcsIdle;
            msg = STR_EMPTYSPACE;
        }
    }
    else if (arg == lcsQuery)
    {
        if (calStep == 1)
        {
            // Step 2: Stick Range Cal
            newStep = lcsAskConfirm;
            msg = "Moved to all Sides/Corners?";
            gyro.StickLimitCalibration(false); // Start
        }
        else if (calStep == 2)
        {
            // Step 3: Done
            newStep = lcsAskConfirm;
            msg = "Calibration Done";
            gyro.StickLimitCalibration(true);
            gyro.reload();
            sendCommandResponse((commandParameter *)item, newStep, msg);
            return;
        }
        else
        {
            msg = STR_EMPTYSPACE;
            newStep = lcsIdle;
        }
    }
    else if (arg == lcsCancel)
    {
        gyro.reload(); // Reactivate Gyro if cancelation
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }
    else // idle
    {
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }

    // DBGLN("Calibrating Workflow RETURN: newStep=[%d],msg=[%s], calStep=[%d]",newStep,msg,calStep);
    sendCommandResponse((commandParameter *)item, newStep, msg);
}

// ------------------- Flight Mode Settings ----------------------
static selectionParameter luaGyroFMode_Select = {
    {"F-Mode ->", CRSF_TEXT_SELECTION},
    0, // value
    fmodes,
    STR_EMPTYSPACE
};

static selectionParameter luaGyroFMode_UseRate = {
    {"Use Rate", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    STR_EMPTYSPACE
};

static selectionParameter luaGyroFMode_StickPri = {
    {"Stick Priority", CRSF_TEXT_SELECTION},
    0, // value
    "100%;75%;50%;25%",
    STR_EMPTYSPACE
};

static stringParameter luaGyroFMode_AngLimitSubHeader = {
    {"--- Angles ---", CRSF_INFO},
    STR_EMPTYSPACE
};

static int8Parameter luaGyroFMode_AngLimitPitch = {
    {"Limit Pitch", CRSF_UINT8},
    {
        {
            (uint8_t)10, // value, not zero-based
            10,          // min
            50,          // max
        }
    },
    " deg"
};

static int8Parameter luaGyroFMode_AngLimitRoll = {
    {"Limit Roll", CRSF_UINT8},
    {
        {
            (uint8_t)30, // value, not zero-based
            30,          // min
            90,          // max
        }
    },
    " deg"
};

static stringParameter luaGyroFMode_TrimSubHeader = {
    {"--- Trims ---", CRSF_INFO},
    STR_EMPTYSPACE
};

static int8Parameter luaGyroFMode_TrimPitch = {
    {"Trim Pitch", CRSF_INT8},
    {
        {
            (uint8_t) 0,    // value
            (uint8_t) 226U, // min (226U = -30)
            (uint8_t) +30,  // max
        }
    },
    " deg (+Up)"
};

static int8Parameter luaGyroFMode_TrimRoll = {
    {"Trim Roll", CRSF_INT8},
    {
        {
            (uint8_t) 0,    // value
            (uint8_t) 226U, // min  (226U = -30)
            (uint8_t) +30,  // max
        }
    },
    " deg (+Left)"
};

static stringParameter luaGyroFMode_Gain_SubHeader = {
    {"-- Gains --", CRSF_INFO},
    STR_EMPTYSPACE
};

static int8Parameter luaGyroFMode_GainPitch = {
    {"Gain Pitch", CRSF_UINT8},
    {
        {
            (uint8_t)1,    // value
            0,             // min
            250            // max
        }
    },
    STR_EMPTYSPACE
};

static int8Parameter luaGyroFMode_GainRoll = {
    {"Gain Roll", CRSF_UINT8},
    {
        {
            (uint8_t)1,    // value
            0,             // min
            250            // max
        }
    },
    STR_EMPTYSPACE
};

static int8Parameter luaGyroFMode_GainYaw = {
    {"Gain Yaw", CRSF_UINT8},
    {
        {
            (uint8_t)1,    // value
            0,             // min
            250            // max
        }
    },
    STR_EMPTYSPACE
};


static void luaparamGyroFMode_UseRate(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.useRate = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_StickPri(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.stickPri = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_AngLimitPitch(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.maxAnglePitch = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_AngLimitRoll(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.maxAngleRoll = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_TrimPitch(propertiesCommon *item, int8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.trimPitch = gyro_trim_encode(arg);
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_TrimRoll(propertiesCommon *item, int8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.trimRoll = gyro_trim_encode(arg);
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_GainPitch(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.gainPitch = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_GainRoll(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.gainRoll = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}

static void luaparamGyroFMode_GainYaw(propertiesCommon *item, uint8_t arg) {
    const gyro_mode_t f_mode = (gyro_mode_t) (luaGyroFMode_Select.value + GYRO_MODE_RATE); // Relative to RATE
    rx_config_gyro_fmode_t newFm;
    newFm.raw = gyroConfig->GetGyroFMode(f_mode)->raw;
    newFm.val.gainYaw = arg;
    gyroConfig->SetGyroFModeRaw(f_mode, newFm.raw);
    gyro.reload();
}
#endif // USE_GYRO

//---------------------------- WiFi -----------------------------

//---------------------------- Output Mapping -----------------------------

static folderParameter luaMappingFolder = {
    {"Output Mapping", CRSF_FOLDER},
};

static int8Parameter luaMappingChannelOut = {
    {"Output Ch", CRSF_UINT8},
    {
        {
            (uint8_t)5,       // value - start on AUX1, value is 1-16, not zero-based
            1,                // min
            PWM_MAX_CHANNELS, // max
        }
    },
    STR_EMPTYSPACE
};

static int8Parameter luaMappingChannelIn = {
    {"Input Ch", CRSF_UINT8},
    {
        {
            0,                 // value
            1,                 // min
            CRSF_NUM_CHANNELS, // max
        }
    },
    STR_EMPTYSPACE
};

static selectionParameter luaMappingOutputMode = {
    {"Output Mode", CRSF_TEXT_SELECTION},
    0, // value
    pwmModes,
    STR_EMPTYSPACE
};

static selectionParameter luaMappingInverted = {
    {"Invert", CRSF_TEXT_SELECTION},
    0, // value
    "Off;On",
    STR_EMPTYSPACE
};

static commandParameter luaSetFailsafe = {
    {"Set Failsafe Pos", CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

#if defined(PLATFORM_ESP32)
const char STR_US[] = " us";
static int16Parameter luaMappingChannelLimitMin = {
    {"Limit Min us", CRSF_UINT16},
    {
        {
            GYRO_US_MIN, // value
            GYRO_US_MIN, // min
            1501, // max
        }
    },
    STR_US
};

static int16Parameter luaMappingChannelLimitMax = {
    {"Limit Max us", CRSF_INT16},
    {
        {
            GYRO_US_MAX, // value
            1501, // min
            GYRO_US_MAX, // max
        }
    },
    STR_US
};

static int16Parameter luaMappingChannelCenter = {
    {"Center us", CRSF_INT16},
    {
        {
            1500, // value
            1000, // min
            2000, // max
        }
    },
    STR_US
};
#endif


//---------------------------- Output Mapping -----------------------------

static selectionParameter luaBindStorage = {
    {"Bind Storage", CRSF_TEXT_SELECTION},
    0, // value
    "Persistent;Volatile;Returnable;Administered",
    STR_EMPTYSPACE
};

static commandParameter luaBindMode = {
    {STR_EMPTYSPACE, CRSF_COMMAND},
    lcsIdle, // step
    STR_EMPTYSPACE
};

static uint8_t sanitizePwmMode(uint8_t mode)
{
    return OPT_PWM_OUT_ONLY && mode >= somSerial ? som50Hz : mode;
}

void RXEndpoint::luaparamMappingChannelOut(propertiesCommon *item, uint8_t arg)
{
    bool sclAssigned = false;
    bool sdaAssigned = false;
#if defined(PLATFORM_ESP32)
    bool serial1rxAssigned = false;
    bool serial1txAssigned = false;
#endif

    const char *no1Option = ";";
    const char *no2Options = ";;";
    const char *serial_RX = ";Serial RX";
    const char *serial_TX = ";Serial TX";
    const char *i2c_SCL = ";I2C SCL;";
    const char *i2c_SDA = ";;I2C SDA";
    const char *i2c_BOTH = ";I2C SCL;I2C SDA";
#if defined(PLATFORM_ESP32)
    const char *serial1_RX = ";Serial2 RX;";
    const char *serial1_TX = ";;Serial2 TX";
    const char *serial1_BOTH = ";Serial2 RX;Serial2 TX";
    const char *dshot = ";DShot;DShot 3D";
#endif

    const char *pModeString;

    // find out if use once only modes have already been assigned
    for (uint8_t ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ch++)
    {
        if (ch == (arg - 1))
            continue;

        eServoOutputMode mode = (eServoOutputMode)config.GetPwmChannel(ch)->val.mode;

        if (mode == somSCL)
            sclAssigned = true;

        if (mode == somSDA)
            sdaAssigned = true;

#if defined(PLATFORM_ESP32)
        if (mode == somSerial1RX)
            serial1rxAssigned = true;

        if (mode == somSerial1TX)
            serial1txAssigned = true;
#endif
    }

    setUint8Value(&luaMappingChannelOut, arg);

    // When the selected output channel changes, update the available PWM modes for that pin
    // Truncate the select options before the ; following On/Off
    pwmModes[50] = '\0';

#if defined(PLATFORM_ESP32)
    // DShot output (2 options)
    // ;DShot;DShot3D
    if (GPIO_PIN_PWM_OUTPUTS[arg - 1] != 0) // DShot doesn't work with GPIO0, exclude it
    {
        pModeString = dshot;
    }
    else
#endif
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);

    // SerialIO outputs (1 option)
    // ;[Serial RX] | [Serial TX]
    if (!OPT_PWM_OUT_ONLY && GPIO_PIN_PWM_OUTPUTS[arg - 1] == U0RXD_GPIO_NUM)
    {
        pModeString = serial_RX;
    }
    else if (!OPT_PWM_OUT_ONLY && GPIO_PIN_PWM_OUTPUTS[arg - 1] == U0TXD_GPIO_NUM)
    {
        pModeString = serial_TX;
    }
    else
    {
        pModeString = no1Option;
    }
    strcat(pwmModes, pModeString);

    // I2C pins (2 options)
    // ;[I2C SCL] ;[I2C SDA]
    if (!OPT_PWM_OUT_ONLY && (GPIO_PIN_SCL != UNDEF_PIN || GPIO_PIN_SDA != UNDEF_PIN))
    {
        // If the target defines SCL/SDA then those pins MUST be used
        if (GPIO_PIN_PWM_OUTPUTS[arg - 1] == GPIO_PIN_SCL)
        {
            pModeString = i2c_SCL;
        }
        else if (GPIO_PIN_PWM_OUTPUTS[arg - 1] == GPIO_PIN_SDA)
        {
            pModeString = i2c_SDA;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    else if (!OPT_PWM_OUT_ONLY)
    {
        // otherwise allow any pin to be either SCL or SDA but only once
        if (sclAssigned && !sdaAssigned)
        {
            pModeString = i2c_SDA;
        }
        else if (sdaAssigned && !sclAssigned)
        {
            pModeString = i2c_SCL;
        }
        else if (!sclAssigned && !sdaAssigned)
        {
            pModeString = i2c_BOTH;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    else
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);

    // nothing to do for unsupported somPwm mode
    strcat(pwmModes, no1Option);

#if defined(PLATFORM_ESP32)
    // secondary Serial pins (2 options)
    // ;[SERIAL2 RX] ;[SERIAL2_TX]
    if (!OPT_PWM_OUT_ONLY && (GPIO_PIN_SERIAL1_RX != UNDEF_PIN || GPIO_PIN_SERIAL1_TX != UNDEF_PIN))
    {
        // If the target defines Serial2 RX/TX then those pins MUST be used
        if (GPIO_PIN_PWM_OUTPUTS[arg - 1] == GPIO_PIN_SERIAL1_RX)
        {
            pModeString = serial1_RX;
        }
        else if (GPIO_PIN_PWM_OUTPUTS[arg - 1] == GPIO_PIN_SERIAL1_TX)
        {
            pModeString = serial1_TX;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    else if (!OPT_PWM_OUT_ONLY)
    { // otherwise allow any pin to be either RX or TX but only once
        if (serial1txAssigned && !serial1rxAssigned)
        {
            pModeString = serial1_RX;
        }
        else if (serial1rxAssigned && !serial1txAssigned)
        {
            pModeString = serial1_TX;
        }

        else if (!serial1rxAssigned && !serial1txAssigned)
        {
            pModeString = serial1_BOTH;
        }
        else
        {
            pModeString = no2Options;
        }
    }
    else
    {
        pModeString = no2Options;
    }
    strcat(pwmModes, pModeString);
#endif

    // trim off trailing semicolons (assumes pwmModes has at least 1 non-semicolon)
    for (auto lastPos = strlen(pwmModes) - 1; pwmModes[lastPos] == ';'; lastPos--)
    {
        pwmModes[lastPos] = '\0';
    }

    // update the related fields to represent the selected channel
    const rx_config_pwm_t *pwmCh = config.GetPwmChannel(luaMappingChannelOut.properties.u.value - 1);
    setUint8Value(&luaMappingChannelIn, pwmCh->val.inputChannel + 1);
    setTextSelectionValue(&luaMappingOutputMode, sanitizePwmMode(pwmCh->val.mode));
    setTextSelectionValue(&luaMappingInverted, pwmCh->val.inverted);
}

static void luaparamMappingChannelIn(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_t newPwmCh;
    newPwmCh.raw = config.GetPwmChannel(ch)->raw;
    newPwmCh.val.inputChannel = arg - 1; // convert 1-16 -> 0-15

    config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

static void configureSerialPin(uint8_t sibling, uint8_t oldMode, uint8_t newMode)
{
    for (int ch = 0; ch < GPIO_PIN_PWM_OUTPUTS_COUNT; ch++)
    {
        if (GPIO_PIN_PWM_OUTPUTS[ch] == sibling)
        {
            // Retain as much of the sibling's current config as possible
            rx_config_pwm_t siblingPinConfig;
            siblingPinConfig.raw = config.GetPwmChannel(ch)->raw;

            // If the new mode is serial, the sibling is also forced to serial
            if (newMode == somSerial)
            {
                siblingPinConfig.val.mode = somSerial;
            }
            // If the new mode is not serial, and the sibling is serial, set the sibling to PWM (50Hz)
            else if (siblingPinConfig.val.mode == somSerial)
            {
                siblingPinConfig.val.mode = som50Hz;
            }

            config.SetPwmChannelRaw(ch, siblingPinConfig.raw);
            break;
        }
    }

    if (oldMode != newMode)
    {
        deferExecutionMillis(100, []() {
            reconfigureSerial();
        });
    }
}

static void luaparamMappingOutputMode(propertiesCommon *item, uint8_t arg)
{
    UNUSED(item);
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_t newPwmCh;
    newPwmCh.raw = config.GetPwmChannel(ch)->raw;
    uint8_t oldMode = newPwmCh.val.mode;
    newPwmCh.val.mode = sanitizePwmMode(arg);

    // Check if pin == 1/3 and do other pin adjustment accordingly
    if (GPIO_PIN_PWM_OUTPUTS[ch] == 1)
    {
        configureSerialPin(3, oldMode, newPwmCh.val.mode);
    }
    else if (GPIO_PIN_PWM_OUTPUTS[ch] == 3)
    {
        configureSerialPin(1, oldMode, newPwmCh.val.mode);
    }
    config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

static void luaparamMappingInverted(propertiesCommon *item, uint8_t arg)
{
    UNUSED(item);
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_t newPwmCh;
    newPwmCh.raw = config.GetPwmChannel(ch)->raw;
    newPwmCh.val.inverted = arg;

    config.SetPwmChannelRaw(ch, newPwmCh.raw);
}

void RXEndpoint::luaparamSetFailsafe(propertiesCommon *item, uint8_t arg)
{
    commandStep_e newStep;
    const char *msg;
    if (arg == lcsClick)
    {
        newStep = lcsAskConfirm;
        msg = "Set failsafe to curr?";
    }
    else if (arg == lcsConfirmed)
    {
        // This is generally not seen by the user, since we'll disconnect to commit config
        // and the handset will send another lcdQuery that will overwrite it with idle
        newStep = lcsExecuting;
        msg = "Setting failsafe";
        servoCurrentToFailsafeConfig();
    }
    else
    {
        newStep = lcsIdle;
        msg = STR_EMPTYSPACE;
    }

    sendCommandResponse((commandParameter *)item, newStep, msg);
}

#if defined(PLATFORM_ESP32)
static void luaparamMappingChannelLimitMin(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_limits_t limits;
    limits.raw = gyroConfig->GetPwmChannelLimits(ch)->raw;
    limits.val.min = (uint16_t)luaMappingChannelLimitMin.properties.u.value;
    gyroConfig->SetPwmChannelLimitsRaw(ch, limits.raw);
}

static void luaparamMappingChannelLimitMax(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_limits_t limits;
    limits.raw = gyroConfig->GetPwmChannelLimits(ch)->raw;
    limits.val.max = luaMappingChannelLimitMax.properties.u.value;
    gyroConfig->SetPwmChannelLimitsRaw(ch, limits.raw);
}

static void luaparamMappingChannelCenter(propertiesCommon *item, uint8_t arg)
{
    const uint8_t ch = luaMappingChannelOut.properties.u.value - 1;
    rx_config_pwm_limits_t limits;
    limits.raw = gyroConfig->GetPwmChannelLimits(ch)->raw;
    limits.val.mid = luaMappingChannelCenter.properties.u.value;
    gyroConfig->SetPwmChannelLimitsRaw(ch, limits.raw);
}
#endif

static void luaparamSetPower(propertiesCommon *item, uint8_t arg)
{
    UNUSED(item);
    uint8_t newPower = arg + POWERMGNT::getMinPower();
    if (newPower > POWERMGNT::getMaxPower())
    {
        newPower = PWR_MATCH_TX;
    }

    config.SetPower(newPower);
    // POWERMGNT::setPower() will be called in updatePower() in the main loop
}

void RXEndpoint::registerParameters()
{
    registerParameter(&luaSerialProtocol, [](propertiesCommon *item, uint8_t arg) {
        config.SetSerialProtocol((eSerialProtocol)arg);
        if (config.IsModified())
        {
            deferExecutionMillis(100, []() {
                reconfigureSerial();
            });
        }
    });

#if defined(PLATFORM_ESP32)
    if (RX_HAS_SERIAL1)
    {
        registerParameter(&luaSerial1Protocol, [](propertiesCommon *item, uint8_t arg) {
            config.SetSerial1Protocol((eSerial1Protocol)arg);
            if (config.IsModified())
            {
                deferExecutionMillis(100, []() {
                    reconfigureSerial1();
                });
            }
        });
    }
#endif

    registerParameter(&luaSBUSFailsafeMode, [](propertiesCommon *item, uint8_t arg) {
        config.SetFailsafeMode((eFailsafeMode)arg);
    });

    registerParameter(&luaTargetSysId, [](propertiesCommon *item, uint8_t arg) {
        config.SetTargetSysId((uint8_t)arg);
    });
    registerParameter(&luaSourceSysId, [](propertiesCommon *item, uint8_t arg) {
        config.SetSourceSysId((uint8_t)arg);
    });

    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
    {
        registerParameter(&luaAntennaMode, [](propertiesCommon *item, uint8_t arg) {
            config.SetAntennaMode(arg);
        });
    }

    if (GPIO_PIN_ANT_GROUP != UNDEF_PIN)
    {
        registerParameter(&luaAntennaGroup, [](propertiesCommon *item, uint8_t arg) {
            config.SetAntennaGroup(arg);
        });
    }

    if (POWERMGNT::getMinPower() != POWERMGNT::getMaxPower())
    {
        filterOptions(&luaTlmPower, POWERMGNT::getMinPower(), POWERMGNT::getMaxPower(), strPowerLevels);
        strcat(strPowerLevels, ";MatchTX ");
        registerParameter(&luaTlmPower, &luaparamSetPower);
    }

    // Teamrace
    registerParameter(&luaTeamraceFolder);
    registerParameter(&luaTeamraceChannel, [](propertiesCommon *item, uint8_t arg) { config.SetTeamraceChannel(arg + AUX2); }, luaTeamraceFolder.common.id);
    registerParameter(&luaTeamracePosition, [](propertiesCommon *item, uint8_t arg) { config.SetTeamracePosition(arg); }, luaTeamraceFolder.common.id);

    if (OPT_HAS_SERVO_OUTPUT)
    {
        luaparamMappingChannelOut(&luaMappingOutputMode.common, luaMappingChannelOut.properties.u.value);
        registerParameter(&luaMappingFolder);
        registerParameter(&luaMappingChannelOut,
            [&](propertiesCommon *item, uint8_t arg) {
                luaparamMappingChannelOut(item, arg);
#if defined(PLATFORM_ESP32)
                // Update Gyro limits when Output channel changes
                const rx_config_pwm_limits_t *limits = gyroConfig->GetPwmChannelLimits(luaMappingChannelOut.properties.u.value - 1);
                setUint16Value(&luaMappingChannelLimitMin, (uint16_t)limits->val.min);
                setUint16Value(&luaMappingChannelLimitMax, (uint16_t)limits->val.max);
                setUint16Value(&luaMappingChannelCenter, (uint16_t)limits->val.mid);
#endif
            },
            luaMappingFolder.common.id
        );
        registerParameter(&luaMappingChannelIn, &luaparamMappingChannelIn, luaMappingFolder.common.id);
        registerParameter(&luaMappingOutputMode, &luaparamMappingOutputMode, luaMappingFolder.common.id);
        registerParameter(&luaMappingInverted, &luaparamMappingInverted, luaMappingFolder.common.id);
        registerParameter(&luaSetFailsafe, [&](propertiesCommon *item, uint8_t arg) {
            luaparamSetFailsafe(item, arg);
        });

#if defined(PLATFORM_ESP32)
        if (OPT_HAS_GYRO && gyroDetected())
        {
            DBGLN("RxPratameters.registerParameters(): Setting up GYRO LUA");
            // -- Servo Output Limits
            registerParameter(&luaMappingChannelLimitMin, &luaparamMappingChannelLimitMin, luaMappingFolder.common.id);
            registerParameter(&luaMappingChannelLimitMax, &luaparamMappingChannelLimitMax, luaMappingFolder.common.id);
            registerParameter(&luaMappingChannelCenter, &luaparamMappingChannelCenter, luaMappingFolder.common.id);

            registerParameter(&luaGyroMainFolder);
            // ----- Gyro Main
            registerParameter(&luaGyroEnabled,
                [&](propertiesCommon *item, uint8_t arg) {
                    gyroConfig->SetGyroEnabled((bool)arg);
                    gyro.reload();
                    updateParameters(); // Update Everything
                },
                luaGyroMainFolder.common.id
            );

            registerParameter(&luaGyroStatus, nullptr, luaGyroMainFolder.common.id);
            registerParameter(&luaGyroIMUStatus, nullptr, luaGyroMainFolder.common.id);
            registerParameter(&luaGyro_Warning, nullptr, luaGyroMainFolder.common.id);
            

            registerParameter(&luaGyroGainFactor,
                [&](propertiesCommon *item, uint8_t arg) {
                    gyroConfig->SetGyroGainFactor((gyro_gain_factor_t) arg);
                    gyro.reload();
                },
                luaGyroMainFolder.common.id
            );

            registerParameter(&luaGyroMainRefresh,
                [this](propertiesCommon *item, uint8_t arg) {
                    // Main Refresh Page
                    updateParameters();
                    sendCommandResponse((commandParameter *)item, lcsIdle, "Refresh");
                },
                luaGyroMainFolder.common.id
            );

            registerParameter(&luaGyroModelFolder, nullptr, luaGyroMainFolder.common.id);
            registerParameter(&luaGyroModesFolder, nullptr, luaGyroModelFolder.common.id);
            registerParameter(&luaGyroOutputFolder, nullptr, luaGyroModelFolder.common.id);
            registerParameter(&luaGyroQuickSetupFolder, nullptr, luaGyroModelFolder.common.id);
            registerParameter(&luaGyroSettingsFolder, nullptr, luaGyroMainFolder.common.id);
            registerParameter(&luaGyroFModeFolder, nullptr, luaGyroSettingsFolder.common.id);
            registerParameter(&luaGyroCalibrationFolder, nullptr, luaGyroSettingsFolder.common.id);
            registerParameter(&luaGyroRxOrientationFolder, nullptr, luaGyroCalibrationFolder.common.id);
            registerParameter(&luaGyroPIDFolder, nullptr, luaGyroSettingsFolder.common.id);

            // ----- Gyro Model->Modes
            registerParameter(&luaGyroModePos1, [&](propertiesCommon *item, uint8_t arg) { gyroConfig->SetGyroModePos(0, (gyro_mode_t)arg); }, luaGyroModesFolder.common.id);
            registerParameter(&luaGyroModePos2, [&](propertiesCommon *item, uint8_t arg) { gyroConfig->SetGyroModePos(1, (gyro_mode_t)arg); }, luaGyroModesFolder.common.id);
            registerParameter(&luaGyroModePos3, [&](propertiesCommon *item, uint8_t arg) { gyroConfig->SetGyroModePos(2, (gyro_mode_t)arg); }, luaGyroModesFolder.common.id);
            registerParameter(&luaGyroModePos4, [&](propertiesCommon *item, uint8_t arg) { gyroConfig->SetGyroModePos(3, (gyro_mode_t)arg); }, luaGyroModesFolder.common.id);
            registerParameter(&luaGyroModePos5, [&](propertiesCommon *item, uint8_t arg) { gyroConfig->SetGyroModePos(4, (gyro_mode_t)arg); }, luaGyroModesFolder.common.id);

            // ----- Gyro Model->Output
            registerParameter(&luaGyroOutputCh_Select, [&](propertiesCommon *item, uint8_t arg) { luaparamGyroOutputCh_Select(item, arg); }, luaGyroOutputFolder.common.id);
            registerParameter(&luaGyroOutputCh_Mode, [&](propertiesCommon *item, uint8_t arg) { luaparamGyroOutputCh_Mode(item, arg); }, luaGyroOutputFolder.common.id);
            registerParameter(&luaGyroOutputCh_Master, [&](propertiesCommon *item, uint8_t arg) { luaparamGyroOutputCh_Master(item, arg); }, luaGyroOutputFolder.common.id);
            registerParameter(&luaGyroOutputCh_Inverted, [&](propertiesCommon *item, uint8_t arg) { luaparamGyroOutputCh_Inverted(item, arg); }, luaGyroOutputFolder.common.id);

            // Hide/unhide lines depending if we are edition a Ch surface or Gain/Mode
            luaparamGyroOutputCh_Select(&luaGyroOutputCh_Select.common, luaGyroOutputCh_Select.properties.u.value);

            // ----- Gyro Settings->PID
            registerParameter(&luaGyroPID_Select_Group,
                [&](propertiesCommon *item, uint8_t arg) {
                    setTextSelectionValue(&luaGyroPID_Select_Group, arg);
                    setTextSelectionValue(&luaGyroPID_Select_Axis, 0); // Reset to X axis
                    // Reload Values
                    updateParameters();
                },
                luaGyroPIDFolder.common.id
            );

            registerParameter(&luaGyroPID_Select_Axis,
                [&](propertiesCommon *item, uint8_t arg) {
                    setTextSelectionValue(&luaGyroPID_Select_Axis, arg);
                    // Reload Values
                    updateParameters();
                },
                luaGyroPIDFolder.common.id
                );

            registerParameter(&luaGyroPID_RateP, &luaparamGyroPID_RateP, luaGyroPIDFolder.common.id);
            registerParameter(&luaGyroPID_RateI, &luaparamGyroPID_RateI, luaGyroPIDFolder.common.id);
            registerParameter(&luaGyroPID_RateD, &luaparamGyroPID_RateD, luaGyroPIDFolder.common.id);

            // ----- Gyro -> Settings -> Calibration -> RxOrientation
            registerParameter(&luaGyroAutoOrientation,
                [this](propertiesCommon *item, uint8_t arg) {
                    luaparamGyroOrientationCal(item, arg);
                    // Reload Values
                    setTextSelectionValue(&luaGyroOrientationH, gyroConfig->GetGyroOrientationH());
                    setTextSelectionValue(&luaGyroOrientationV, gyroConfig->GetGyroOrientationV());
                },
                luaGyroRxOrientationFolder.common.id
            );

            registerParameter(&luaGyroOrientationH,
                [this](propertiesCommon *item, uint8_t arg) {
                    setTextSelectionValue(&luaGyroOrientationH, arg);
                    gyroConfig->SetGyroOrientation(luaGyroOrientationH.value,luaGyroOrientationV.value);
                    gyro.reload();
                },
                luaGyroRxOrientationFolder.common.id
            );

            registerParameter(&luaGyroOrientationV,
                [this](propertiesCommon *item, uint8_t arg) {
                    setTextSelectionValue(&luaGyroOrientationV, arg);
                    gyroConfig->SetGyroOrientation(luaGyroOrientationH.value,luaGyroOrientationV.value);
                    gyro.reload();
                },
                luaGyroRxOrientationFolder.common.id
            );

            // Update orientation Options now that we know what Gyro type do we have
            setTextSelectionOptions(&luaGyroOrientationH, (char *)(OPT_HAS_GYRO_MPU6050 ? gyroRxOrientationsHR : gyroRxOrientationsRM));
            setTextSelectionOptions(&luaGyroOrientationV, (char *)(OPT_HAS_GYRO_MPU6050 ? gyroRxOrientationsHR : gyroRxOrientationsRM));

            // ----- Gyro -> Settings - > Calibration -> Gyro Calibration
            registerParameter(&luaGyroCalibration, [this](propertiesCommon *item, uint8_t arg) { luaparamGyroCalibration(item, arg); }, luaGyroCalibrationFolder.common.id);

            // ----- Gyro -> Model -> Stick Calibration
            registerParameter(&luaGyroStickCal, [this](propertiesCommon *item, uint8_t arg) { luaparamGyroStickCal(item, arg); }, luaGyroCalibrationFolder.common.id);

            // ----- Gyro -> Model -> Quick Setup
            // Wing Type
            registerParameter(&luaGyroQuickSetup_wingType_Select, [this](propertiesCommon *item, uint8_t arg) { setTextSelectionValue(&luaGyroQuickSetup_wingType_Select, arg); }, luaGyroQuickSetupFolder.common.id);
            // Tail Type
            registerParameter(&luaGyroQuickSetup_tailType_Select, [this](propertiesCommon *item, uint8_t arg) { setTextSelectionValue(&luaGyroQuickSetup_tailType_Select, arg); }, luaGyroQuickSetupFolder.common.id);

            // Execute
            registerParameter(&luaGyroQuickPreset,
                [this](propertiesCommon *item, uint8_t arg) {
                    luaparamGyroQuickPreset(item, arg);
                    updateParameters();
                },
                luaGyroQuickSetupFolder.common.id
            );

            // Gyro Settings-> FMode Folder
            registerParameter(&luaGyroFMode_Select,
                [&](propertiesCommon *item, uint8_t arg) {
                    setTextSelectionValue(&luaGyroFMode_Select, arg);
                    // Reload Values
                    updateParameters();
                },
                luaGyroFModeFolder.common.id
            );

            registerParameter(&luaGyroFMode_UseRate, &luaparamGyroFMode_UseRate, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_StickPri, &luaparamGyroFMode_StickPri, luaGyroFModeFolder.common.id);

            registerParameter(&luaGyroFMode_AngLimitSubHeader, nullptr, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_AngLimitPitch, &luaparamGyroFMode_AngLimitPitch, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_AngLimitRoll, &luaparamGyroFMode_AngLimitRoll, luaGyroFModeFolder.common.id);

            registerParameter(&luaGyroFMode_TrimSubHeader, nullptr, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_TrimPitch, &luaparamGyroFMode_TrimPitch, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_TrimRoll, &luaparamGyroFMode_TrimRoll, luaGyroFModeFolder.common.id);

            registerParameter(&luaGyroFMode_Gain_SubHeader, nullptr, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_GainRoll, &luaparamGyroFMode_GainRoll, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_GainPitch, &luaparamGyroFMode_GainPitch, luaGyroFModeFolder.common.id);
            registerParameter(&luaGyroFMode_GainYaw, &luaparamGyroFMode_GainYaw, luaGyroFModeFolder.common.id);

            DBGLN("RxPratameters.registerParameters(): GYRO LUA Done");
        } // OPT_HAS_GYRO
#endif
    }

    registerParameter(&luaBindStorage, [](propertiesCommon *item, uint8_t arg) {
        config.SetBindStorage((rx_config_bindstorage_t)arg);
    });
    registerParameter(&luaBindMode, [this](propertiesCommon *item, uint8_t arg) {
        // Complete when TX polls for status i.e. going back to idle, because we're going to lose connection
        if (arg == lcsQuery)
        {
            deferExecutionMillis(200, EnterBindingModeSafely);
        }
        sendCommandResponse(&luaBindMode, arg < 5 ? lcsExecuting : lcsIdle, arg < 5 ? "Entering..." : "");
    });

    registerParameter(&luaModelNumber);
    registerParameter(&luaELRSversion);
}

static void updateBindModeLabel()
{
    if (config.IsOnLoan())
        luaBindMode.common.name = "Return Model";
    else
        luaBindMode.common.name = "Enter Bind Mode";
}

#if defined(PLATFORM_ESP32)
static void getFormatedGyroStatus(char *buffer)
{
    sprintf(buffer, "v%2.2f / %d", GYRO_CODE_VERSION, gyroConfig->GetGyroConfigVersion());
}

static void getFormatedGyroIMUStatus(char *buffer)
{
    if (gyro.ahrs->getImuDriver())
    {
        sprintf(buffer, "IMU %s", gyro.ahrs->getImuDriver()->GetMPUName());
    }
    else
    {
        sprintf(buffer, "IMU --");
    }
}
#endif

void RXEndpoint::updateParameters()
{
    // TODO FArzu:  Should we update LUA values if the RX is disconnected??
    // Or even better, do the check at the caller "devRXLua.cpp" method "event"
    // i.e:  if (connectionState != connected) return;

    setTextSelectionValue(&luaSerialProtocol, config.GetSerialProtocol());
#if defined(PLATFORM_ESP32)
    if (RX_HAS_SERIAL1)
    {
        setTextSelectionValue(&luaSerial1Protocol, config.GetSerial1Protocol());
    }
#endif

    setTextSelectionValue(&luaSBUSFailsafeMode, config.GetFailsafeMode());

    if (GPIO_PIN_ANT_CTRL != UNDEF_PIN)
    {
        setTextSelectionValue(&luaAntennaMode, config.GetAntennaMode());
    }

    if (GPIO_PIN_ANT_GROUP != UNDEF_PIN)
    {
        setTextSelectionValue(&luaAntennaGroup, config.GetAntennaGroup());
    }

    if (MinPower != MaxPower)
    {
        // The last item (for MatchTX) will be MaxPower - MinPower + 1
        uint8_t luaPwrVal = (config.GetPower() == PWR_MATCH_TX) ? POWERMGNT::getMaxPower() + 1 : config.GetPower();
        setTextSelectionValue(&luaTlmPower, luaPwrVal - POWERMGNT::getMinPower());
    }

    // Teamrace
    setTextSelectionValue(&luaTeamraceChannel, config.GetTeamraceChannel() - AUX2);
    setTextSelectionValue(&luaTeamracePosition, config.GetTeamracePosition());

    if (OPT_HAS_SERVO_OUTPUT)
    {
        const rx_config_pwm_t *pwmCh = config.GetPwmChannel(luaMappingChannelOut.properties.u.value - 1);
        setUint8Value(&luaMappingChannelIn, pwmCh->val.inputChannel + 1);
        setTextSelectionValue(&luaMappingOutputMode, sanitizePwmMode(pwmCh->val.mode));
        setTextSelectionValue(&luaMappingInverted, pwmCh->val.inverted);
    }

#if defined(PLATFORM_ESP32)
    if (OPT_HAS_GYRO && gyroDetected() && connectionState == connected)
    {
        DBGLN("updateParameters(): Updating Gyro LUA values");

        auto gyroEnabled = gyroConfig->GetGyroEnabled();
        setTextSelectionValue(&luaGyroEnabled, gyroEnabled);

        getFormatedGyroStatus(gyroStatusStr);
        luaGyroStatus.common.name = gyroStatusStr; // Change Title
        setStringValue(&luaGyroStatus, gyroStatus[gyro.getStatus()]);

        getFormatedGyroIMUStatus(gyroIMUStatusStr);
        if (gyro.getStatus() == GYRO_STATUS_NEED_STICK_CAL)
        {
            sprintf(gyroIMUErrorStr, "%s", gyro.lastErrorText);
        }
        else
        {
            sprintf(gyroIMUErrorStr, "re=%ld,ie=%ld", gyro.ahrs->read_errors, gyro.ahrs->int_errors);
        }
        luaGyroIMUStatus.common.name = gyroIMUStatusStr; // Change Title
        setStringValue(&luaGyroIMUStatus, gyroIMUErrorStr);

        setTextSelectionValue(&luaGyroGainFactor, gyroConfig->GetGyroGainFactor());

        const rx_config_pwm_limits_t *limits = gyroConfig->GetPwmChannelLimits(luaMappingChannelOut.properties.u.value - 1);
        setUint16Value(&luaMappingChannelLimitMin, (uint16_t)limits->val.min);
        setUint16Value(&luaMappingChannelLimitMax, (uint16_t)limits->val.max);
        setUint16Value(&luaMappingChannelCenter, (uint16_t)limits->val.mid);

        const rx_config_gyro_channel_t *gyroChOut = gyroConfig->GetGyroChannel(luaGyroOutputCh_Select.properties.u.value - 1);
        setTextSelectionValue(&luaGyroOutputCh_Mode, gyroChOut->val.output_mode);
        setTextSelectionValue(&luaGyroOutputCh_Master, gyroChOut->val.master);
        setTextSelectionValue(&luaGyroOutputCh_Inverted, gyroChOut->val.inverted);

        const rx_config_gyro_mode_pos_t *gyroModeSwitch = gyroConfig->GetGyroModePos();
        setTextSelectionValue(&luaGyroModePos1, gyroModeSwitch->val.pos1);
        setTextSelectionValue(&luaGyroModePos2, gyroModeSwitch->val.pos2);
        setTextSelectionValue(&luaGyroModePos3, gyroModeSwitch->val.pos3);
        setTextSelectionValue(&luaGyroModePos4, gyroModeSwitch->val.pos4);
        setTextSelectionValue(&luaGyroModePos5, gyroModeSwitch->val.pos5);

        const auto group = (gyro_pidgroup_t)luaGyroPID_Select_Group.value;
        auto axis = (gyro_axis_t)luaGyroPID_Select_Axis.value;

        // No Axis with Group MADGWICK
        if (group == GYRO_PID_GROUP_MADGWICK)
        {
            axis = GYRO_AXIS_ROLL; // 0
        }
        LUA_FIELD_VISIBLE(luaGyroPID_Select_Axis, group != GYRO_PID_GROUP_MADGWICK);
        const rx_config_gyro_PID_t *gyroPIDs = gyroConfig->GetGyroPID(group, axis);
        setUint8Value(&luaGyroPID_RateP, gyroPIDs->val.p);
        setUint8Value(&luaGyroPID_RateI, gyroPIDs->val.i);
        setUint8Value(&luaGyroPID_RateD, gyroPIDs->val.d);

        setTextSelectionValue(&luaGyroOrientationH, gyroConfig->GetGyroOrientationH());
        setTextSelectionValue(&luaGyroOrientationV, gyroConfig->GetGyroOrientationV());

        const gyro_mode_t fm = (gyro_mode_t)(luaGyroFMode_Select.value + GYRO_MODE_RATE); // Start at 1
        const rx_config_gyro_fmode_t *fMode = gyroConfig->GetGyroFMode(fm);

        setTextSelectionValue(&luaGyroFMode_UseRate, fMode->val.useRate);
        LUA_FIELD_VISIBLE(luaGyroFMode_UseRate, gyroIsVisible(fm, GYRO_UI_USE_RATE)); // All Except Rate

        setTextSelectionValue(&luaGyroFMode_StickPri, fMode->val.stickPri);
        LUA_FIELD_VISIBLE(luaGyroFMode_StickPri, gyroIsVisible(fm, GYRO_UI_STICK_PRIORITY));

        setUint8Value(&luaGyroFMode_AngLimitPitch, fMode->val.maxAnglePitch);
        setUint8Value(&luaGyroFMode_AngLimitRoll, fMode->val.maxAngleRoll);

        bool limitsVisible = gyroIsVisible(fm, GYRO_UI_MAX_ANGLE);
        LUA_FIELD_VISIBLE(luaGyroFMode_AngLimitSubHeader, limitsVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_AngLimitPitch, limitsVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_AngLimitRoll, limitsVisible);

        setUint8Value(&luaGyroFMode_TrimPitch, gyro_trim_decode(fMode->val.trimPitch));
        setUint8Value(&luaGyroFMode_TrimRoll, gyro_trim_decode(fMode->val.trimRoll));

        bool trimVisible = gyroIsVisible(fm, GYRO_UI_TRIMS);
        LUA_FIELD_VISIBLE(luaGyroFMode_TrimSubHeader, trimVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_TrimPitch, trimVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_TrimRoll, trimVisible);

        setUint8Value(&luaGyroFMode_GainPitch, fMode->val.gainPitch);
        setUint8Value(&luaGyroFMode_GainRoll, fMode->val.gainRoll);
        setUint8Value(&luaGyroFMode_GainYaw, fMode->val.gainYaw);

        bool gainsVisible = gyroIsVisible(fm, GYRO_UI_GAINS);
        LUA_FIELD_VISIBLE(luaGyroFMode_Gain_SubHeader, gainsVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_GainPitch, gainsVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_GainRoll, gainsVisible);
        LUA_FIELD_VISIBLE(luaGyroFMode_GainYaw, gainsVisible);
    }
#endif

    if (config.GetModelId() == 255)
    {
        setStringValue(&luaModelNumber, "Off");
    }
    else
    {
        itoa(config.GetModelId(), modelString, 10);
        setStringValue(&luaModelNumber, modelString);
    }
    setTextSelectionValue(&luaBindStorage, config.GetBindStorage());
    updateBindModeLabel();

    if (config.GetSerialProtocol() == PROTOCOL_MAVLINK)
    {
        setUint8Value(&luaSourceSysId, config.GetSourceSysId() == 0 ? 255 : config.GetSourceSysId()); // display Source sysID if 0 display 255 to mimic logic in SerialMavlink.cpp
        setUint8Value(&luaTargetSysId, config.GetTargetSysId() == 0 ? 1 : config.GetTargetSysId());   // display Target sysID if 0 display 1 to mimic logic in SerialMavlink.cpp
        LUA_FIELD_SHOW(luaSourceSysId)
        LUA_FIELD_SHOW(luaTargetSysId)
    }
    else
    {
        LUA_FIELD_HIDE(luaSourceSysId)
        LUA_FIELD_HIDE(luaTargetSysId)
    }
}
#endif
