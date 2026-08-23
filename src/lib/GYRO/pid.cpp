#include "pid.h"

#include "targets.h"
#include "logging.h"

PID::PID(const float max, const float min, const float Kp, const float Ki, const float Kd)
    : output(0),
      error(0),
      pv(0),
      Pout(0),
      Iout(0),
      Dout(0),
      _maximum(max),
      _minimum(min),
      _Kp(Kp),
      _Ki(Ki),
      _Kd(Kd),
      _integral(0),
      setpoint(0),
      tau(0),
      prevMeasurement(0),
      last_update(micros())
{
}

void PID::configure(const float Kp, const float Ki, const float Kd, const float max, const float min)
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
    prevMeasurement = 0;
}

float PID::calculate(const float _setpoint, const float _pv)
{
    const unsigned long now = micros();
    uint32_t t_delta = now - last_update;
    t_delta = t_delta == 0 ? 1 : t_delta; // Stop any chance of div/0
    const float _dt = 1.0f / (float)t_delta;
    last_update = now;

    // Store input for debugging
    setpoint = _setpoint;
    pv = _pv;

    // Calculate error
    const float current_error = setpoint - pv;

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
    Dout = -(2.0f * _Kd * (pv - prevMeasurement) /* Note: derivative on measurement, therefore minus sign in front of equation! */
             + (2.0f * tau - (float)t_delta) * Dout) /
           (2.0f * tau + (float)t_delta);
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

#if defined(DEBUG_LOG)
void PID::debug(const char *header) const
{
    DBGLN("%s Setpoint: %5.2f PV: %5.2f I:%5.2f D:%5.2f Error: %5.2f Out: %5.2f",
        header, setpoint, pv, Iout, Dout, error, output);
}
#endif