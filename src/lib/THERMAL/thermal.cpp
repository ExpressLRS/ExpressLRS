#ifdef HAS_THERMAL
#include "thermal.h"
#include "logging.h"

#ifdef HAS_THERMAL_LM75A
#include "lm75a.h"
LM75A lm75a;
#endif
#if defined(PLATFORM_ESP32_S3)
#include "driver/temp_sensor.h"
#endif

uint8_t thermal_threshold_data[] = {
    THERMAL_FAN_DEFAULT_HIGH_THRESHOLD,
    THERMAL_FAN_DEFAULT_LOW_THRESHOLD,
    THERMAL_FAN_ALWAYS_ON_HIGH_THRESHOLD,
    THERMAL_FAN_ALWAYS_ON_LOW_THRESHOLD,
    THERMAL_FAN_OFF_HIGH_THRESHOLD,
    THERMAL_FAN_OFF_LOW_THRESHOLD
};

int thermal_status = THERMAL_STATUS_FAIL;
static bool use_internal_sensor = false;

void Thermal::init()
{
    int status = -1;
    if (OPT_HAS_THERMAL_LM75A)
    {
        status = lm75a.init();
    }
#if defined(PLATFORM_ESP32_S3)
    if (status == -1)
    {
        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor.dac_offset = TSENS_DAC_L2;  //TSENS_DAC_L2 is default   L4(-40℃ ~ 20℃), L2(-10℃ ~ 80℃) L1(20℃ ~ 100℃) L0(50℃ ~ 125℃)
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();
        use_internal_sensor = true;
    }
#else
    if(status == -1)
    {
        ERRLN("Thermal failed!");
        return;
    }
    else
#endif
    {
        DBGLN("Thermal OK!");
        temp_value = 0;
        thermal_status = THERMAL_STATUS_NORMAL;
        update_threshold(0);
    }
}

void Thermal::handle()
{
    temp_value = read_temp();
}

uint8_t Thermal::read_temp()
{
    if(thermal_status != THERMAL_STATUS_NORMAL)
    {
        ERRLN("thermal not ready!");
        return 0;
    }
    if (OPT_HAS_THERMAL_LM75A)
    {
        return lm75a.read_lm75a();
    }

#if defined(PLATFORM_ESP32_S3)
    float result = 0;
    temp_sensor_read_celsius(&result);
    return result;
#else
    return 0;
#endif
}

void Thermal::update_threshold(int index)
{
    static int prevIndex = -1;
    if (index == prevIndex)
    {
        return;
    }
    prevIndex = index;
    if(thermal_status != THERMAL_STATUS_NORMAL)
    {
        ERRLN("thermal not ready!");
        return;
    }
    int size = sizeof(thermal_threshold_data)/sizeof(thermal_threshold_data[0]);
    if(index > size/2)
    {
        ERRLN("thermal index out of range!");
        return;
    }
    uint8_t high = thermal_threshold_data[2*index];
    uint8_t low = thermal_threshold_data[2*index+1];
    if (OPT_HAS_THERMAL_LM75A)
    {
        lm75a.update_lm75a_threshold(high, low);
    }
}

#endif