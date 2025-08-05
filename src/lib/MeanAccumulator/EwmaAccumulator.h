#pragma once

#include <cmath>  // for std::sqrt
#include <type_traits>
#include <algorithm> // for std::max, std::min

/// @brief A class that computes Exponentially Weighted Moving Average (EWMA)
///        and Exponentially Weighted Standard Deviation (EWSTD)
///        using minimal memory, suitable for streaming time-series data.
///
/// @tparam IncrementType  Input value type (e.g., int or float)
/// @tparam NoValueReturn  Return value when no samples have been added
template <typename IncrementType, IncrementType NoValueReturn>
class EwmaAccumulator
{
public:
    /// @brief Constructor to initialize smoothing factor α (for mean) and β (for variance)
    /// @param alpha Smoothing factor for EWMA (float: 0 < alpha ≤ 1, equivalent to int: 1-255)
    /// @param beta  Smoothing factor for variance (float: 0 < beta ≤ 1, equivalent to int: 1-255)
    EwmaAccumulator(float alpha = 0.01f, float beta = 0.001f)
        : _hasValue(false), _mean(0), _variance(0), _count(0)
    {
        setAlphaBeta(alpha, beta);
    }

    /// @brief Sets alpha and beta, handling integer/fixed-point or float
    void setAlphaBeta(float alpha, float beta)
    {
        // Store as integer in [1,255] (0 not allowed)
        _alpha_i = static_cast<uint8_t>(std::max(1, std::min(255, static_cast<int>(alpha * 255.0f + 0.5f))));
        _beta_i  = static_cast<uint8_t>(std::max(1, std::min(255, static_cast<int>(beta  * 255.0f + 0.5f))));
    }

    /// @brief Adds a new value to the accumulator
    /// @param val The new input value
    void add(IncrementType val)
    {
        // Integer/fixed-point version: alpha/beta in [1,255], scale is 255
        // Increase precision by 8x (3 bits) for _mean and _variance
        constexpr int SCALE = 255;
        constexpr int PRECISION = 1 << 3; // 8

        if (_hasValue)
        {
            // Use higher precision for mean and variance
            // All calculations are done in (value * PRECISION) space
            int32_t prevMean = _mean;
            // _mean = alpha * val + (1-alpha) * _mean
            // All values are scaled by PRECISION
            int32_t val_scaled = static_cast<int32_t>(val) * PRECISION;
            _mean = (_alpha_i * val_scaled + (SCALE - _alpha_i) * _mean + (SCALE/2)) / SCALE;

            int32_t delta = val_scaled - prevMean;
            // _variance = beta * _variance + (1-beta) * (delta^2)
            int32_t delta_squared = delta * delta;
            // delta^2 is in (value^2 * PRECISION^2), so divide by PRECISION to keep _variance in (value^2 * PRECISION)
            _variance = (_beta_i * _variance + (SCALE - _beta_i) * delta_squared + (SCALE/2)) / SCALE;
        }
        else
        {
            // Store initial value with extra precision
            _mean = static_cast<int32_t>(val) * PRECISION;
            _variance = 0;
            _hasValue = true;
        }
        ++_count;
    }

    /// @brief Returns the current EWMA value
    float mean() const
    {
        // Remove extra precision bits using consistent PRECISION constant
        constexpr int PRECISION = 1 << 3; // 8
        return _hasValue ? static_cast<float>(_mean) / PRECISION : NoValueReturn;
    }

    /// @brief Returns the estimated standard deviation
    float standardDeviation() const
    {
        if (!_hasValue) return NoValueReturn;
        // _variance is in (value^2 * PRECISION)
        // Convert to float, divide by PRECISION, then sqrt
        constexpr int PRECISION = 1 << 3; // 8
        float variance = static_cast<float>(_variance) / PRECISION;
        return std::sqrt(variance);
    }

    /// @brief Resets the internal state
    void reset()
    {
        _mean = 0;
        _variance = 0;
        _hasValue = false;
        _count = 0;
    }

    /// @brief Returns whether any values have been added
    bool hasValue() const
    {
        return _hasValue;
    }

    /// @brief Returns the number of accumulated values
    unsigned int count() const
    {
        return _count;
    }

    /// @brief Sets the count to a specific value (useful for testing or manual adjustment)
    void setCount(unsigned int count)
    {
        _count = count;
    }

private:
    // Smoothing factors: uint8_t for integer/fixed-point
    uint8_t _alpha_i;
    uint8_t _beta_i;
    bool _hasValue;         ///< Indicates if the accumulator has been initialized
    int32_t _mean;      ///< Exponentially weighted moving average (with extra precision for integer)
    int32_t _variance;  ///< Exponentially weighted variance (with extra precision for integer)
    uint32_t _count;    ///< Number of accumulated values
};
