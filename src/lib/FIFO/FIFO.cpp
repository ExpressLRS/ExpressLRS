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
 * Modified/Ammended by Alessandro Carcione 2020
 */
#include "FIFO.h"
#include "../../src/debug.h"

FIFO::FIFO()
{
    head = 0;
    tail = 0;
    numElements = 0;
}

FIFO::~FIFO()
{
}

void ICACHE_RAM_ATTR FIFO::push(uint8_t data)
{
    if (numElements == FIFO_SIZE)
    {
        DEBUG_PRINTLN(("CRITICAL ERROR: Buffer full, will flush"));
        //this->flush();
        this->popBytes(nullptr, numElements);
        return;
    }
    else
    {
        //Increment size
        numElements++;

        //Only move the tail if there is more than one element
        if (numElements > 1)
        {
            //Increment tail location
            tail++;

            //Make sure tail is within the bounds of the array
            tail %= FIFO_SIZE;
        }

        //Store data into array
        buffer[tail] = data;
    }
}

void ICACHE_RAM_ATTR FIFO::pushBytes(uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        FIFO::push(data[i]);
    }
}

uint8_t ICACHE_RAM_ATTR FIFO::pop()
{
    if (numElements == 0)
    {
        //    DEBUG_PRINTLN(F("Buffer empty"));
        return 0;
    }
    else
    {
        //Decrement size
        numElements--;

        uint8_t data = buffer[head];

        if (numElements >= 1)
        {
            //Move head up one position
            head++;

            //Make sure head is within the bounds of the array
            head %= FIFO_SIZE;
        }

        return data;
    }
}

void ICACHE_RAM_ATTR FIFO::popBytes(uint8_t *data, int len)
{
    for (int i = 0; i < len; i++)
    {
        data[i] = FIFO::pop();
    }
}

uint8_t ICACHE_RAM_ATTR FIFO::peek()
{
    if (numElements == 0)
    {
        //    DEBUG_PRINTLN(F("Buffer empty"));
        return 0;
    }
    else
    {
        uint8_t data = buffer[head];
        return data;
    }
}

int ICACHE_RAM_ATTR FIFO::size()
{
    return numElements;
}

void ICACHE_RAM_ATTR FIFO::flush()
{
    //memset(buffer, 0x00, FIFO_SIZE);
    head = 0;
    tail = 0;
    numElements = 0;
}
