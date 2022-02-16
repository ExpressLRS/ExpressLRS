#pragma once

template <typename StorageType, typename IncrementType, IncrementType NoValueReturn>
class MeanAccumulator
{
public:
    void add(IncrementType val)
    {
        _accumulator += val;
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

private:
    StorageType _accumulator;
    size_t _count;
};