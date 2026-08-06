#pragma once
#include "gyro_types.h"

extern int8_t gyro_trim_encode(int8_t n);
extern int8_t gyro_trim_decode(int8_t n);

class Mode_Base
{
    public:
        virtual void    initialize(gyro_mode_t mode);
        virtual void    calculate_pid(float input_rpt[], float gyro_rpy[], float ang_rpy[]);
        #if defined(DEBUG_LOG)
        virtual void    printState();
        #endif
        virtual uint16_t applyCorrection(uint8_t ch, gyro_output_channel_function_t channel_function, float command, bool inverted);
    protected:
        gyro_mode_t  mode;
        rx_config_gyro_fmode_t fm_settings;
        gyro_stick_priority_t stick_priority = STICK_PRIORITY_50;
        
        float input_rpy[3];
        float stick_pri[3];
        bool  ignore_input[3];
        float corr[3];
};