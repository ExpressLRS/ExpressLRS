#pragma once

#include <functional>

void deferExecutionMicros(unsigned long us, std::function<void()> f);
void executeDeferredFunction(unsigned long now);

static inline void deferExecutionMillis(unsigned long ms, std::function<void()> f)
{
    deferExecutionMicros(ms * 1000, f);
}