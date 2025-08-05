#pragma once

#include <cstdint>
#include <cmath>

/// @brief Naive fixed-size accumulator using a 256-byte circular buffer.
///        Computes mean and standard deviation over a moving window of the last 256 samples.
class StdevAccumulator
{
public:
    static const size_t WINDOW_SIZE = 64;

    StdevAccumulator()
    {
        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            _buffer[i] = 0;
        }
        _updated = false;
        _index = 0;
        _count = 0;
        _lastMean = 0;
        _lastStdev = 0;
    }

    /// @brief Adds a new uint8_t sample into the circular buffer.
    ///        If full, the oldest value is overwritten (moving window).
    void add(int8_t value)
    {
        _buffer[_index] = value;
        _index = (_index + 1) % WINDOW_SIZE;

        if (_count < WINDOW_SIZE)
        {
            ++_count;
        }

        _updated = true;
    }

    /// @brief Computes and returns the mean of the current buffer contents.
    /// @return Mean as float, or -1.0f if no data
    float mean()
    {
        if (_count == 0)
        {
            return -1.0f;
        }

        if (!_updated)
        {
            return _lastMean;
        }

        uint32_t sum = 0;
        for (size_t i = 0; i < _count; ++i)
        {
            sum += _buffer[i];
        }

        _lastMean = static_cast<float>(sum) / _count;
        return _lastMean;
    }

    /// @brief Computes and returns the standard deviation of the current buffer contents.
    /// @return Standard deviation as float, or -1.0f if not enough data
    float standardDeviation()
    {
        if (_count < 2)
        {
            return -1.0f;
        }

        if (!_updated)
        {
            return _lastStdev;
        }

        float m = mean();
        float variance = 0.0f;

        for (size_t i = 0; i < _count; ++i)
        {
            float delta = static_cast<float>(_buffer[i]) - m;
            variance += delta * delta;
        }

        variance /= (_count - 1);
        _lastStdev = std::sqrt(variance);
        return _lastStdev;
    }

    /// @brief Returns the number of valid entries (up to 256)
    uint16_t getCount() const
    {
        return _count;
    }

private:
    bool _updated;
    int8_t _buffer[WINDOW_SIZE];  ///< Circular buffer
    uint16_t _index;                ///< Write index (oldest value is here)
    uint16_t _count;                ///< Valid entry count (≤ 256)
    float _lastMean;
    float _lastStdev;
};
