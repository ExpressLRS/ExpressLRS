#pragma once

#include <cstdint>
#include <cmath>

// run in IRAM for speed up when running on ESP32
#if defined(PLATFORM_ESP32) || defined(PLATFORM_ESP32S3)
#include "esp_attr.h"
#define STDEV_INLINE inline IRAM_ATTR
#else
#define STDEV_INLINE inline
#endif

// Fixed-point arithmetic constants
// Using 16-bit fixed-point with 8 fractional bits (Q8.8 format)
#define FIXED_POINT_SHIFT 8
#define FIXED_POINT_SCALE (1 << FIXED_POINT_SHIFT)
#define FIXED_POINT_MASK (FIXED_POINT_SCALE - 1)

// Fast integer square root using binary search
// algorithm adopted from https://en.wikipedia.org/wiki/Methods_of_computing_square_roots#Binary_numeral_system_(base_2)
STDEV_INLINE uint16_t fast_sqrt_uint(uint32_t x) {
    if (x == 0) {
        return 0;
    }

    uint32_t op = x;
    uint32_t res = 0;
    uint32_t one = 1u << 30; // second-to-top bit set

    // Bring 'one' down to the highest power-of-four <= op
    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op -= res + one;
            res = (res >> 1) + one;
        } else {
            res >>= 1;
        }
        one >>= 2;
    }

    return static_cast<uint16_t>(res);
}

/// @brief Efficient fixed-size mean/stdev incremental accumulator using a circular buffer (of size WINDOW_SIZE).
class StdevAccumulator
{
public:
    static const size_t WINDOW_SIZE = 48;

    StdevAccumulator()
    {
        reset();
    }

    /// @brief Adds a new int8_t sample into the circular buffer.
    ///        If full, the oldest value is overwritten (moving window).
    ///        Only minimal work should be done here for interrupt safety.
    void ICACHE_RAM_ATTR add(int8_t value)
    {
        int8_t old = _buffer[_index];
        _buffer[_index] = value;

        if (_count < WINDOW_SIZE)
        {
            ++_count;
            _sum += value;
            _sumSq += static_cast<int32_t>(value) * value;
        }
        else
        {
            // Remove the old value, add the new one
            _sum += value - old;
            int16_t newSq = static_cast<int16_t>(value) * value;
            int16_t oldSq = static_cast<int16_t>(old) * old;
            _sumSq += newSq - oldSq;
        }

        _index = (_index + 1) % WINDOW_SIZE;
        _dirty = true;
    }

    /// @brief Computes and returns the mean of the current buffer contents.
    /// @return Mean as float, or 0.0f if no data
    float mean()
    {
        if (_count == 0)
        {
            return 0.0f;
        }

        if (!_dirty)
        {
            return static_cast<float>(_lastMeanFixed) / FIXED_POINT_SCALE;
        }

        // Fixed-point division using bit shifting
        _lastMeanFixed = (_sum << FIXED_POINT_SHIFT) / _count;
        // Don't clear _dirty here, as stdev may still need update
        return static_cast<float>(_lastMeanFixed) / FIXED_POINT_SCALE;
    }

    /// @brief Computes and returns the standard deviation of the current buffer contents.
    /// @return Standard deviation as float, or 0.0f if not enough data
    float standardDeviation()
    {
        if (_count < 2)
        {
            return 0.0f;
        }

        if (!_dirty)
        {
            return static_cast<float>(_lastStdevFixed) / FIXED_POINT_SCALE;
        }

        // Calculate mean in fixed-point
        int32_t meanFixed = (_sum << FIXED_POINT_SHIFT) / _count;
        // Keep cached mean consistent when clearing _dirty below
        _lastMeanFixed = meanFixed;

        // Calculate E[x^2] in fixed-point (mean of squares)
        int32_t ex2Fixed = (_sumSq << FIXED_POINT_SHIFT) / _count;

        // Calculate (E[x])^2 in fixed-point (square of mean)
        int32_t exFixedSquared = (meanFixed * meanFixed) >> FIXED_POINT_SHIFT;

        // Variance = E[x^2] - (E[x])^2
        int32_t varianceFixed = ex2Fixed - exFixedSquared;

        // Apply Bessel's correction: multiply by n/(n-1) for calculating sample variance
        varianceFixed = (varianceFixed * _count) / (_count - 1);

        // Clamp variance to zero if negative due to rounding
        if (varianceFixed < 0)
            varianceFixed = 0;

        // Calculate square root using integer math
        // Note: sqrt(variance * 256) = sqrt(variance) * 16, so we need to scale back to 256
        _lastStdevFixed = fast_sqrt_uint(varianceFixed) << 4;
        _dirty = false;
        return static_cast<float>(_lastStdevFixed) / FIXED_POINT_SCALE;
    }

    /// @brief Returns the number of valid entries (up to WINDOW_SIZE)
    uint16_t getCount() const
    {
        return _count;
    }

    /// @brief Resets the accumulator to its initial state.
    void reset()
    {
        std::fill(_buffer, _buffer + WINDOW_SIZE, 0);
        _index = 0;
        _count = 0;
        _sum = 0;
        _sumSq = 0;
        _lastMeanFixed = 0;
        _lastStdevFixed = 0;
        _dirty = true;
    }

private:
    int8_t _buffer[WINDOW_SIZE];    ///< Circular buffer
    uint16_t _index;                ///< Write index (next position to write)
    uint16_t _count;                ///< Valid entry count (<= WINDOW_SIZE)
    int32_t _sum;                   ///< Sum of current buffer values
    int32_t _sumSq;                 ///< Sum of squares of current buffer values
    int32_t _lastMeanFixed;         ///< Last calculated mean in fixed-point format
    int32_t _lastStdevFixed;        ///< Last calculated stdev in fixed-point format
    bool _dirty;                    ///< True if cached mean/stdev are invalid (set to true when add() is called)
};
