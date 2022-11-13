/*
 * FIFO Buffer
 * Implementation uses arrays to conserve memory
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015 Daniel Eisterhold
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 *
 *
 * Modified/Ammended by Alessandro Carcione 2020
 */

#pragma once
#include "targets.h"
#include "logging.h"

template <uint32_t FIFO_SIZE>
class FIFO_GENERIC
{
private:
    uint32_t head;
    uint32_t tail;
    uint32_t numElements;
    uint8_t buffer[FIFO_SIZE] = {0};
#if defined(PLATFORM_ESP32)
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#define ENTER_CRITICAL  portENTER_CRITICAL(&mux)
#define EXIT_CRITICAL   portEXIT_CRITICAL(&mux)
#elif defined(PLATFORM_ESP8266)
#define ENTER_CRITICAL  noInterrupts()
#define EXIT_CRITICAL   interrupts()
#elif defined(PLATFORM_STM32)
#define ENTER_CRITICAL  noInterrupts()
#define EXIT_CRITICAL   interrupts()
#else
#define ENTER_CRITICAL
#define EXIT_CRITICAL
#endif

public:
    FIFO_GENERIC()
    {
        head = 0;
        tail = 0;
        numElements = 0;
        memset(buffer, 0, FIFO_SIZE);
    }

    void push(const uint8_t data)
    {
        if (numElements == FIFO_SIZE)
        {
            ERRLN("Buffer full, will flush");
            flush();
            return;
        }
        ENTER_CRITICAL;
        buffer[tail] = data;
        tail = (tail + 1) % FIFO_SIZE;
        numElements++;
        EXIT_CRITICAL;
    }

    void pushBytes(const uint8_t *data, uint32_t len)
    {
        if (numElements + len > FIFO_SIZE)
        {
            ERRLN("Buffer full, will flush");
            flush();
            return;
        }
        ENTER_CRITICAL;
        for (uint32_t i = 0; i < len; i++)
        {
            buffer[tail] = data[i];
            tail = (tail + 1) % FIFO_SIZE;
        }
        numElements += len;
        EXIT_CRITICAL;
    }

    uint8_t pop()
    {
        if (numElements == 0)
        {
            // DBGLN(F("Buffer empty"));
            return 0;
        }
        ENTER_CRITICAL;
        uint8_t data = buffer[head];
        head = (head + 1) % FIFO_SIZE;
        numElements--;
        EXIT_CRITICAL;
        return data;
    }

    void popBytes(uint8_t *data, uint32_t len)
    {
        if (numElements < len)
        {
            // DBGLN(F("Buffer underrun"));
            flush();
            return;
        }
        ENTER_CRITICAL;
        for (uint32_t i = 0; i < len; i++)
        {
            data[i] = buffer[head];
            head = (head + 1) % FIFO_SIZE;
        }
        numElements -= len;
        EXIT_CRITICAL;
    }

    uint8_t peek()
    {
        if (numElements == 0)
        {
            // DBGLN(F("Buffer empty"));
            return 0;
        }
        else
        {
            uint8_t data = buffer[head];
            return data;
        }
    }

    uint16_t free()
    {
        return FIFO_SIZE - numElements;
    }

    uint16_t size()
    {
        return numElements;
    }

    void pushSize(uint16_t size)
    {
        push(size & 0xFF);
        push((size >> 8) & 0xFF);
    }

    uint16_t peekSize()
    {
        if (size() > 1)
        {
            return (uint16_t)buffer[head] + ((uint16_t)buffer[(head + 1) % FIFO_SIZE] << 8);
        }
        return 0;
    }

    uint16_t popSize()
    {
        if (size() > 1)
        {
            return (uint16_t)pop() + ((uint16_t)pop() << 8);
        }
        return 0;
    }

    void flush()
    {
        ENTER_CRITICAL;
        head = 0;
        tail = 0;
        numElements = 0;
        EXIT_CRITICAL;
    }
};