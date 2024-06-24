#pragma once

#include "targets.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

class NullStream : public Stream
{
  public:
    int available() override
    {
        return 0;
    }

    void flush() override
    {
    }

    int peek() override
    {
        return -1;
    }

    int read() override
    {
        return -1;
    }

    size_t write(uint8_t u_Data) override
    {
        UNUSED(u_Data);
        return 0x01;
    }

    size_t write(const uint8_t *buffer, size_t size) override
    {
        UNUSED(buffer);
        return size;
    }
};

#if defined(PLATFORM_STM32)
inline const char *strchrnul(const char *pos, const char find)
{
    const char *semi = strchr(pos, find);
    if (semi == nullptr)
    {
        semi = pos + strlen(pos);
    }
    return semi;
}
#endif

