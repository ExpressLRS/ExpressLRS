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

class FIFO
{
public:
    // If this is 256 then the compiler can optimise out the modulo because the head/tail are uint8_t types
    static const uint16_t FIFO_SIZE = 256;

private:
    uint8_t head;
    uint8_t tail;
    uint16_t numElements;
    uint8_t buffer[FIFO_SIZE] = {0};
#if defined(PLATFORM_ESP32)
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
#endif

public:
    FIFO();
    ~FIFO();

    // Push a single byte to the FIFO, FIFO is flushed if this byte will not fit and the byte is not pushed
    void push(const uint8_t data);

    // Push all bytes to FIFO, if all the bytes will not fit then the FIFO is flushed and no bytes are pushed
    void pushBytes(const uint8_t *data, uint8_t len);

    // Pop a single byte (returns 0 if no bytes left)
    uint8_t pop();

    // Pops 'len' bytes into the buffer pointed to by 'data'. If there are not enough bytes in the FIFO
    // then the FIFO is flush and the bytes are not read
    void popBytes(uint8_t *data, uint8_t len);

    // return the first byte in the FIFO without removing it
    uint8_t peek();

    // return the number of bytes in the FIFO
    uint16_t size();

    // reset the FIFO back to empty
    void flush();

    // returns true if the number of bytes requested is available in the FIFO
    bool available(uint8_t requiredSize) { return (numElements + requiredSize) < FIFO_SIZE; }

    // Ensure that there is enough room in the FIFO for the requestedSize in bytes.
    // "packets" are popped from the head of the FIFO until there is enough room available.
    // This method assumes that on the FIFO contains length-prefixed data packets.
    bool ensure(uint8_t requiredSize);
};