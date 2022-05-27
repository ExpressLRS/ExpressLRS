#pragma once
#include <common.h>

template <typename StorageType, typename IncrementType, IncrementType NoValueReturn>
class MeanAccumulator
{
public:
    void add(IncrementType val)
    {
        _accumulator += (StorageType)val * DYNPOWER_MEAN_ACCUMULATOR_MUL;
        ++_count;
    }

    IncrementType mean()
    {
        if (_count)
        {
            IncrementType retVal = _accumulator / _count;
            reset();

            return retVal;
        }
        return NoValueReturn;
    }

    void reset()
    {
        _accumulator = 0;
        _count = 0;
    }

    size_t getCount() const
    {
        return _count;
    }

private:
    StorageType _accumulator;
    StorageType _count;
};