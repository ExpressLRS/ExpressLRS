#pragma once

#include <stdint.h>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>

#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <sys/time.h>

#define DEVICE_NAME "testing"
#define RADIO_SX128X 1

// fake pin definitions to satisfy the SX1280 library
#define GPIO_PIN_BUSY UNDEF_PIN
#define GPIO_PIN_NSS_2 UNDEF_PIN
#define OPT_USE_HARDWARE_DCDC false

// fake pin definition to satisfy th MSPVTX  library
#define OPT_HAS_VTX_SPI false
#define GPIO_PIN_SPI_VTX_NSS UNDEF_PIN

typedef uint8_t byte;

#define HEX 16
#define DEC 10

class Stream
{
public:
    Stream() {}
    virtual ~Stream() {}

    // Stream methods
    virtual int available() = 0;
    virtual int read() = 0;
    virtual int peek() = 0;
    virtual void flush() = 0;

    // Print methods
    virtual size_t write(uint8_t c) = 0;
    virtual size_t write(const uint8_t *s, size_t l) = 0;

    int print(const char *s) {return 0;}
    int print(uint8_t s) {return 0;}
    int print(uint8_t s, int radix) {return 0;}
    int println() {return 0;}
    int println(const char *s) {return 0;}
    int println(uint8_t s) {return 0;}
    int println(uint8_t s, int radix) {return 0;}
};

class HardwareSerial: public Stream {
public:
    // Stream methods
    int available() {return 0;}
    int read() {return -1;}
    int peek() {return 0;}
    void flush() {}
    void end() {}
    void begin(int baud) {}
    void enableHalfDuplexRx() {}
    int availableForWrite() {return 256;}

    // Print methods
    size_t write(uint8_t c) {return 1;}
    size_t write(const uint8_t *s, size_t l) {return l;}

    int print(const char *s) {return 0;}
    int print(uint8_t s) {return 0;}
    int print(uint8_t s, int radix) {return 0;}
    int println() {return 0;}
    int println(const char *s) {return 0;}
    int println(uint8_t s) {return 0;}
    int println(uint8_t s, int radix) {return 0;}
};

static HardwareSerial Serial;

inline void interrupts() {}
inline void noInterrupts() {}

inline unsigned long micros() {
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*(uint64_t)1000000+tv.tv_usec;
}

inline void delay(int32_t time) {
    usleep(time);
}

#define bit(x) (1 << (x))
inline unsigned long millis() { return 0; }
inline void delayMicroseconds(int delay) { }
inline char *itoa(int32_t value, char *str, int base) { sprintf(str, "%d", value); return str; }
inline char *utoa(uint32_t value, char *str, int base) { sprintf(str, "%u", value); return str; }
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))

#if defined(_WIN32) || defined(__MINGW32__)
// Flippin BSD functions, not available on windows for tests!
/*
 * Copyright (c) 1998, 2015 Todd C. Miller <millert@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
static inline size_t strlcpy(char *dst, const char *src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';		/* NUL-terminate dst */
		while (*src++)
			;
	}

	return(src - osrc - 1);	/* count does not include NUL */
}

static inline char *stpcpy(char *dst, const char *src)
{
	for (; (*dst = *src) != '\0'; ++src, ++dst);
	return(dst);
}

static inline size_t strlcat(char *dst, const char *src, size_t dsize)
{
	const char *odst = dst;
	const char *osrc = src;
	size_t n = dsize;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end. */
	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = dst - odst;
	n = dsize - dlen;

	if (n-- == 0)
		return(dlen + strlen(src));
	while (*src != '\0') {
		if (n != 0) {
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';

	return(dlen + (src - osrc));	/* count does not include NUL */
}

#define random rand
#define srandom srand
#endif
