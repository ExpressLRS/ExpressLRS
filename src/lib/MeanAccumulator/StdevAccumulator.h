#pragma once

#include <cstdint>
#include <cmath>

/// @brief Efficient fixed-size accumulator using a circular buffer.
///        Computes mean and standard deviation over a moving window of the last WINDOW_SIZE samples.
///        Uses incremental algorithm to minimize calculation time in mean() and standardDeviation().
class StdevAccumulator
{
public:
    static const size_t WINDOW_SIZE = 32;

    StdevAccumulator()
    {
        for (int i = 0; i < WINDOW_SIZE; ++i)
        {
            _buffer[i] = 0;
        }
        _index = 0;
        _count = 0;
        _sum = 0;
        _sumSq = 0;
        _lastMean = 0.0f;
        _lastStdev = 0.0f;
        _lastPrintIndex = 0;
        _dirty = true;
    }

    /// @brief Adds a new int8_t sample into the circular buffer.
    ///        If full, the oldest value is overwritten (moving window).
    ///        Only minimal work is done here for interrupt safety.
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
            // Use int64_t to prevent overflow in the subtraction
            int64_t newSq = static_cast<int64_t>(value) * value;
            int64_t oldSq = static_cast<int64_t>(old) * old;
            _sumSq += static_cast<int32_t>(newSq - oldSq);
        }

        _index = (_index + 1) % WINDOW_SIZE;
        _dirty = true;
    }

    /// @brief Computes and returns the mean of the current buffer contents.
    /// @return Mean as float, or -1.0f if no data
    float mean()
    {
        if (_count == 0)
        {
            return -1.0f;
        }

        if (!_dirty)
        {
            return _lastMean;
        }

        _lastMean = static_cast<float>(_sum) / _count;
        // Don't clear _dirty here, as stdev may still need update
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

        if (!_dirty)
        {
            return _lastStdev;
        }

        float m = mean();
        float meanSq = static_cast<float>(_sumSq) / _count;
        float variance = (meanSq - m * m) * _count / (_count - 1);

        // Clamp variance to zero if negative due to rounding
        if (variance < 0.0f)
            variance = 0.0f;

        _lastStdev = std::sqrt(variance);
        _dirty = false;
        return _lastStdev;
    }

    /// @brief Returns the number of valid entries (up to WINDOW_SIZE)
    uint16_t getCount() const
    {
        return _count;
    }

    void printBuffer()
    {
        if(_lastPrintIndex == _index || _count == 0)
        {
            return;
        }

        // Ensure indices are within valid range
        if (_lastPrintIndex >= WINDOW_SIZE || _index >= WINDOW_SIZE)
        {
            _lastPrintIndex = 0;
            return;
        }

        int16_t start = _lastPrintIndex-1;
        int16_t end = _index-1;

        if(start < 0)
        {
            start += WINDOW_SIZE;
        }
        if(end < 0)
        {
            end += WINDOW_SIZE;
        }

        // Print from start to end, wrapping if needed
        int16_t i = start;
        DBG("BUF: ");
        while (i != end) {
            i = (i + 1) % WINDOW_SIZE;
            DBG("%d ", _buffer[i]);
        }
        DBGLN("");
        _lastPrintIndex = _index;
    }

    void reset()
    {
        _count = 0;
        _sum = 0;
        _sumSq = 0;
        _lastMean = 0.0f;
        _lastStdev = 0.0f;
        _lastPrintIndex = 0;
        _dirty = true;
    }

private:
    int8_t _buffer[WINDOW_SIZE];    ///< Circular buffer
    uint16_t _index;                ///< Write index (next position to write)
    uint16_t _count;                ///< Valid entry count (≤ WINDOW_SIZE)
    int32_t _sum;                   ///< Sum of current buffer values
    int32_t _sumSq;                 ///< Sum of squares of current buffer values
    float _lastMean;
    float _lastStdev;
    int16_t _lastPrintIndex;
    bool _dirty;                    ///< True if mean/stdev need recalculation
};
