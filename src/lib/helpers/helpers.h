#pragma once

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

class NullStream : public Stream
{
  public:
    int available(void)
    {
        return 0;
    }

    void flush(void)
    {
        return;
    }

    int peek(void)
    {
        return -1;
    }

    int read(void)
    {
        return -1;
    }

    size_t write(uint8_t u_Data)
    {
        UNUSED(u_Data);
        return 0x01;
    }

    size_t write(const uint8_t *buffer, size_t size)
    {
        UNUSED(buffer);
        return size;
    }
};
