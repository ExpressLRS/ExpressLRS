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
            _previousMean = _accumulator / _count;
            reset();

            return _previousMean;
        }
        return NoValueReturn;
    }

    IncrementType previousMean()
    {
        return _previousMean;
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
    IncrementType _previousMean;
};