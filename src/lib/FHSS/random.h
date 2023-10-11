#pragma once

#include <cstdint>
#include <limits>

/// @brief Class with static methods for random generation
class randomGeneration {
public:
    /// @brief Sets new seed for random generation
    /// @param newSeed new seed
    static void setSeed(const uint32_t newSeed);

    /// @brief Returns random value in range [0, upper], where upper < 256
    /// @param upper upper bound of random value
    /// @return random value in range [0, upper]
    static uint8_t randomValueByUpperBound(const uint8_t upper);
private:
    /// @brief Seed for random generation
    static uint32_t seed;

    /// @brief Returns random value
    /// @return random value
    static uint16_t random();
};
