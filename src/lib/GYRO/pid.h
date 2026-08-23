#pragma once

class PID
{
public:
    explicit PID(float max = 0.0f, float min = 0.0f, float Kp = 0.0f, float Ki = 0.0f, float Kd = 0.0f);
    float calculate(float setpoint, float pv);
    void reset();
    void configure(float Kp, float Ki, float Kd, float max = 1.0, float min = -1.0);

    float output;

#if defined(DEBUG_LOG)
    void debug(const char *header) const;
#endif
private:
    float error;
    float pv;

    float Pout;
    float Iout;
    float Dout;

    float _maximum;
    float _minimum;
    float _Kp;
    float _Ki;
    float _Kd;
    float _integral;

    float setpoint;

    /* Derivative low-pass filter time constant */
    float tau;
    float prevMeasurement;

    unsigned long last_update;
};
