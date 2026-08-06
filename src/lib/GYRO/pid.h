#pragma once

class PID
{
public:
    PID();
    PID(float max, float min, float Kp, float Ki, float Kd);
    float calculate(float setpoint, float pv);
    void reset();
    void configure(float Kp, float Ki, float Kd, float max = 1.0, float min = -1.0);

    float _maximum;
    float _minimum;
    float _Kp;
    float _Ki;
    float _Kd;
    float error;
    float _integral;
    float setpoint;
    float pv;
    float output;

    float Pout;
    float Iout;
    float Dout;
    unsigned long t_delta;

    /* Derivative low-pass filter time constant */
    float tau;
    float prevMeasurement;

    unsigned long last_update;
};
