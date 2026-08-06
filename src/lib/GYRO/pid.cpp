#include "targets.h"
#include "pid.h"



PID::PID() {
    PID(0,0,0,0,0);
}

PID::PID(float max, float min, float Kp, float Ki, float Kd)
    : _maximum(max),
      _minimum(min),
      _Kp(Kp),
      _Ki(Ki),
      _Kd(Kd),
      error(0),
      _integral(0),
      setpoint(0),
      pv(0),
      output(0),
      tau(0),
      prevMeasurement(0),
      last_update(micros())
{
}

void PID::configure(float Kp, float Ki, float Kd, float max, float min)
{
    _Kp = Kp;
    _Ki = Ki;
    _Kd = Kd * 10;
    _maximum = max;
    _minimum = min;
    tau = 0.02;
}

void PID::reset()
{
    error = 0;
    _integral = 0;
    setpoint = 0;
    pv = 0;
    output = 0;
    Dout = 0;
    Iout = 0;
    last_update = micros();
    prevMeasurement=0;
}

float PID::calculate(float _setpoint, float _pv)
{
    unsigned long now = micros();
    t_delta = now - last_update;
    t_delta = t_delta == 0 ? 1 : t_delta; // Stop any chance of div/0
    float _dt = 1.0 / t_delta;
    last_update = now;

    // Store input for debugging
    setpoint = _setpoint;
    pv = _pv;

    // Calculate error
    float current_error = setpoint - pv;

    // Proportional term
    Pout = _Kp * current_error;

    // Integral term
    _integral += current_error * _dt;

    // Limit the I accumulation within min/max
    _integral = (_integral * _Ki) > _maximum
                    ? _maximum / _Ki
                : (_integral * _Ki) < _minimum
                    ? _minimum / _Ki
                    : _integral;

    Iout = _Ki * _integral;

    // Derivative term

    Dout = -(2.0f * _Kd * (pv - prevMeasurement)	/* Note: derivative on measurement, therefore minus sign in front of equation! */
           + (2.0f * tau - t_delta) * Dout)
           / (2.0f * tau + t_delta);
    prevMeasurement = pv;
    // Calculate total output
    output = Pout + Iout + Dout;

    // Limit output
    if (output > _maximum)
        output = _maximum;
    if (output < _minimum)
        output = _minimum;

    // Save error to previous error
    error = current_error;

    return output;
}
