#pragma once
#include "gyro.h"
#include "mode.h"


class RateController: public Mode_Base
{
    public:
        RateController();
        void    initialize(gyro_mode_t mode);
       
        void    applyFModeSettings(gyro_mode_t fm);
        void    calculate_pid(float input_rpy[], float gyro_rpy[], float ang_rpy[]);
        void    calculate_stick_pri(float input_rpy[]);
        virtual uint16_t applyCorrection(uint8_t ch, gyro_output_channel_function_t channel_function, float command,bool inverted);
        bool    isInverted(float angle_rpy[]);
        bool    isHighPitch(float angle_rpy[]);
        #if defined(DEBUG_LOG)
        void    printState();
        #endif
    protected:
        void    configure_pids(float roll_limit, float pitch_limit, float yaw_limit, const rx_config_gyro_fmode_t *fm);
        void    configure_pid_gains(PID *pid, const rx_config_gyro_PID_t *pid_params, int8_t gain, float max, float min);
        
        PID   pid_roll, pid_pitch, pid_yaw;
        
};
