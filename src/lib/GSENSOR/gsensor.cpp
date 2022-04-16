#ifdef HAS_GSENSOR
#include "gsensor.h"
#include "logging.h"

#ifdef HAS_GSENSOR_STK8xxx
#include "stk8baxx.h"
STK8xxx stk8xxx;
#endif

int gensor_status = GSENSOR_STATUS_FAIL;

#define DATA_SAMPLE_LENGTH  20
#define QUIET_VARIANCE_THRESHOLD    0.0002f
#define QUIET_AVERAGE_THRESHOLD     0.04f

#define FLIPPED_ACTION_THRESHOLD  0.2f

static float x_buffer[DATA_SAMPLE_LENGTH] = {0};
static float y_buffer[DATA_SAMPLE_LENGTH] = {0};
static float z_buffer[DATA_SAMPLE_LENGTH] = {0};
static int sample_counter = 0;
float x_average = 0;
float y_average = 0;
float z_average = 0;

static bool interrupt = false;

#ifdef HAS_SMART_FAN
extern bool is_smart_fan_control;
uint32_t smart_fan_start_time = 0;
#define SMART_FAN_TIME_OUT 5000
#endif

ICACHE_RAM_ATTR void handleGsensorInterrupt()
{
    interrupt = true;
}

void Gsensor::init()
{
    uint8_t id = -1;
#ifdef HAS_GSENSOR_STK8xxx
    id = stk8xxx.STK8xxx_Initialization();
#endif
    if(id == -1)
    {
        ERRLN("Gsensor failed!");
    }
    else
    {
        DBGLN("Gsensor OK with chipid = %x!", id);
        gensor_status = GSENSOR_STATUS_NORMAL;
    }

    if(gensor_status == GSENSOR_STATUS_NORMAL)
    {
#ifdef HAS_GSENSOR_STK8xxx
       stk8xxx.STK8xxx_Anymotion_init();
       pinMode(GPIO_PIN_GSENSOR_INT,INPUT_PULLUP);
       attachInterrupt(digitalPinToInterrupt(GPIO_PIN_GSENSOR_INT), handleGsensorInterrupt, FALLING);
#endif
    }

    system_state = GSENSOR_SYSTEM_STATE_MOVING;
    is_flipped = false;
}

float get_data_average(float *data, int length)
{
    float average = 0;
    for(int i=0;i<length;i++)
    {
        average += *(data+i);
    }
    average = average / length;
    return average;
}

float get_data_variance(float average, float *data, int length)
{
    float variance = 0;
    for(int i=0;i<length;i++)
    {
        float temp = *(data+i) - average;
        temp = temp * temp;
        variance += temp;
    }
    variance = variance / length;
    return variance;
}

bool Gsensor::handleBump(unsigned long now)
{
    float x, y, z;
    static unsigned long startBump = 0;

    if (interrupt)
    {
        interrupt = false;
        if (now - startBump > 20)
        {
            getGSensorData(&x, &y, &z);
            startBump = now;
            return true;
        }
    }
    return false;
}

void Gsensor::handle()
{
    float x, y, z;

    getGSensorData(&x, &y, &z);
#ifdef HAS_SMART_FAN
    if(z < -0.5f)
    {
        is_smart_fan_control = true;
        smart_fan_start_time = millis();
    }
    else
    {
        if(is_smart_fan_control && (millis() - smart_fan_start_time) > SMART_FAN_TIME_OUT)
        {
            is_smart_fan_control = false;
        }
    }
#endif
    if(z > FLIPPED_ACTION_THRESHOLD)
    {
        is_flipped = true;
    }
    else
    {
        is_flipped = false;
    }
    if(system_state == GSENSOR_SYSTEM_STATE_QUIET)
    {
        x = abs(x - x_average);
        y = abs(y - y_average);
        z = abs(z - z_average);
        if((x > QUIET_AVERAGE_THRESHOLD) || (y > QUIET_AVERAGE_THRESHOLD) ||
            (z > QUIET_AVERAGE_THRESHOLD))
        {
            system_state = GSENSOR_SYSTEM_STATE_MOVING;
        }
    }
    else
    {
        x_buffer[sample_counter] = x;
        y_buffer[sample_counter] = y;
        z_buffer[sample_counter] = z;

        if(sample_counter == DATA_SAMPLE_LENGTH -1)
        {
            sample_counter = 0;
            x_average = get_data_average(x_buffer, DATA_SAMPLE_LENGTH);
            y_average = get_data_average(y_buffer, DATA_SAMPLE_LENGTH);
            z_average = get_data_average(z_buffer, DATA_SAMPLE_LENGTH);

            float x_variance = get_data_variance(x_average, x_buffer, DATA_SAMPLE_LENGTH);
            float y_variance = get_data_variance(y_average, y_buffer, DATA_SAMPLE_LENGTH);
            float z_variance = get_data_variance(z_average, z_buffer, DATA_SAMPLE_LENGTH);

            if((x_variance < QUIET_VARIANCE_THRESHOLD) && (y_variance < QUIET_VARIANCE_THRESHOLD) &&
                (z_variance < QUIET_VARIANCE_THRESHOLD))
            {
                system_state = GSENSOR_SYSTEM_STATE_QUIET;
            }
        }
        else
        {
            sample_counter++;
        }
    }
}

void Gsensor::getGSensorData(float *X_DataOut, float *Y_DataOut, float *Z_DataOut)
{
    *X_DataOut = 0;
    *Y_DataOut = 0;
    *Z_DataOut = 0;
    if(gensor_status == GSENSOR_STATUS_NORMAL)
    {
#ifdef HAS_GSENSOR_STK8xxx
        stk8xxx.STK8xxx_Getregister_data(X_DataOut, Y_DataOut, Z_DataOut);
#endif
    }
    else
    {
        ERRLN("Gsensor abnormal status = %d", gensor_status);
    }
}

int Gsensor::getSystemState()
{
    return system_state;
}

bool Gsensor::isFlipped()
{
    return is_flipped;
}

#endif