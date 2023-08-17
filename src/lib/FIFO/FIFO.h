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
class FIFO
{
private:
    uint8_t buffer[FIFO_SIZE] = {0};
    uint32_t head = 0;
    uint32_t tail = 0;
    uint32_t numElements = 0;
#if defined(PLATFORM_ESP32)
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif

public:
    ICACHE_RAM_ATTR void inline lock()
    {
    #if defined(PLATFORM_ESP32)
        portENTER_CRITICAL(&mux);
    #elif defined(PLATFORM_ESP8266)
        noInterrupts();
    #elif defined(PLATFORM_STM32)
        noInterrupts();
    #else
    #endif
    }

    ICACHE_RAM_ATTR void inline unlock()
    {
    #if defined(PLATFORM_ESP32)
        portEXIT_CRITICAL(&mux);
    #elif defined(PLATFORM_ESP8266)
        interrupts();
    #elif defined(PLATFORM_STM32)
        interrupts();
    #else
    #endif
    }

    // Push a single byte to the FIFO, FIFO is flushed if this byte will not fit and the byte is not pushed
    ICACHE_RAM_ATTR void inline push(const uint8_t data)
    {
        if (numElements == FIFO_SIZE)
        {
            ERRLN("Buffer full, will flush");
            flush();
            return;
        }
        else
        {
            numElements++;
            buffer[tail] = data;
            tail = (tail + 1) % FIFO_SIZE;
        }
    }

    // Push all bytes to FIFO, if all the bytes will not fit then the FIFO is flushed and no bytes are pushed
    ICACHE_RAM_ATTR void inline pushBytes(const uint8_t *data, uint8_t len)
    {
        if (numElements + len > FIFO_SIZE)
        {
            ERRLN("Buffer full, will flush");
            flush();
            return;
        }
        for (int i = 0; i < len; i++)
        {
            buffer[tail] = data[i];
            tail = (tail + 1) % FIFO_SIZE;
        }
        numElements += len;
    }

    // Push all bytes to FIFO, if all the bytes will not fit then the FIFO is flushed and no bytes are pushed
    ICACHE_RAM_ATTR void inline atomicPushBytes(const uint8_t *data, uint8_t len)
    {
        lock();
        pushBytes(data, len);
        unlock();
    }

    // Pop a single byte (returns 0 if no bytes left)
    ICACHE_RAM_ATTR uint8_t inline pop()
    {
        if (numElements == 0)
        {
            // DBGLN(F("Buffer empty"));
            return 0;
        }
        numElements--;
        uint8_t data = buffer[head];
        head = (head + 1) % FIFO_SIZE;
        return data;
    }

    // Pops 'len' bytes into the buffer pointed to by 'data'. If there are not enough bytes in the FIFO
    // then the FIFO is flush and the bytes are not read
    ICACHE_RAM_ATTR void inline popBytes(uint8_t *data, uint8_t len)
    {
        if (numElements < len)
        {
            // DBGLN(F("Buffer underrun"));
            flush();
            return;
        }
        numElements -= len;
        for (int i = 0; i < len; i++)
        {
            data[i] = buffer[head];
            head = (head + 1) % FIFO_SIZE;
        }
    }

    // return the first byte in the FIFO without removing it
    ICACHE_RAM_ATTR uint8_t inline peek()
    {
        if (numElements == 0)
        {
            // DBGLN(F("Buffer empty"));
            return 0;
        }
        uint8_t data = buffer[head];
        return data;
    }

    // return the number of bytes in the FIFO
    ICACHE_RAM_ATTR uint16_t inline size()
    {
        return numElements;
    }

    ICACHE_RAM_ATTR void inline pushSize(uint16_t size)
    {
        push(size & 0xFF);
        push((size >> 8) & 0xFF);
    }

    ICACHE_RAM_ATTR uint16_t inline peekSize()
    {
        if (size() > 1)
        {
            return (uint16_t)buffer[head] + ((uint16_t)buffer[(head + 1) % FIFO_SIZE] << 8);
        }
        return 0;
    }

    ICACHE_RAM_ATTR uint16_t inline popSize()
    {
        if (size() > 1)
        {
            return (uint16_t)pop() + ((uint16_t)pop() << 8);
        }
        return 0;
    }

    // reset the FIFO back to empty
    ICACHE_RAM_ATTR void inline flush()
    {
        head = 0;
        tail = 0;
        numElements = 0;
    }

    // returns true if the number of bytes requested is available in the FIFO
    ICACHE_RAM_ATTR bool inline available(uint8_t requiredSize)
    {
        return (numElements + requiredSize) < FIFO_SIZE;
    }

    // Ensure that there is enough room in the FIFO for the requestedSize in bytes.
    // "packets" are popped from the head of the FIFO until there is enough room available.
    // This method assumes that on the FIFO contains length-prefixed data packets.
    ICACHE_RAM_ATTR bool inline ensure(uint8_t requiredSize)
    {
        if(requiredSize > FIFO_SIZE)
        {
            return false;
        }
        while(!available(requiredSize))
        {
            uint8_t len = pop();
            head = (head + len) % FIFO_SIZE;
            numElements -= len;
        }
        return true;
    }
};