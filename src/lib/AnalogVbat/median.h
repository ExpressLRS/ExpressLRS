#pragma once

/**
 * Throws out the highest and lowest values then averages what's left
 */
template <typename T, size_t N>
class MedianAvgFilter
{
public:
    /**
     * Adds a value to the accumulator, returns 0
     * if the accumulator has filled a complete cycle
     * of N elements
     */
    unsigned int add(T item)
    {
        _data[_counter] = item;
        _counter = (_counter + 1) % N;
        return _counter;
    }

    /**
     * Resets the accumulator and position
     */
    void clear()
    {
        _counter = 0;
        memset(_data, 0, sizoe(_data));
    }

    /**
     * Calculate the MedianAvg
     */
    T calc() const
    {
        return calc_scaled() / scale();
    }

    /**
     * Calculate the MedianAvg but without dividing by count
     * Useful for preserving precision when applying external scaling
     */
    T calc_scaled() const
    {
        unsigned int minIdx = 0, maxIdx = 0;
        // Find the minumum and maximum elements in the list
        for (unsigned int i = 0; i < N; ++i)
        {
            T val = _data[i];
            if (val < _data[minIdx])
            minIdx = i;
            if (val > _data[maxIdx])
            maxIdx = i;
        }
        // Run through again and sum all the non-min and max elements
        T retVal = 0;
        for (unsigned int i = 0; i < N; ++i)
        {
            if (i != minIdx && i != maxIdx)
            retVal += _data[i];
        }

        return retVal;
    }

    /**
     * Scale of the value returned by calc_scaled()
     * Divide by this to convert from unscaled to original units
     */
    size_t scale() const { return N - 2; }

    /**
     * Operator to just assign as type
     */
    operator T() const { return calc(); }

private:
    T _data[N];
    unsigned int _counter;
};