#if defined(PLATFORM_ESP32)

#include "SDBulkLink.h"

#include <Arduino.h>
#include <string.h>

static constexpr uint8_t SYNC = 0xEB;
static constexpr uint8_t RESPONSE = 0x80;

static uint16_t crc16(const uint8_t *p, size_t n)
{
    uint16_t crc = 0xFFFF;
    while (n--)
    {
        crc ^= (uint16_t)(*p++) << 8;
        for (int i = 0; i < 8; i++)
            crc = (crc & 0x8000) ? (crc << 1) ^ 0x1021 : (crc << 1);
    }
    return crc;
}

bool SDBulkLink::parse(uint8_t c)
{
    if (rxLen == 0 && c != SYNC)
        return false;
    rx[rxLen++] = c;
    if (rxLen < 3)
        return false;
    const uint16_t flen = rx[1] | (rx[2] << 8);
    if (flen < 3 || flen > maxPayload + 3)
    {
        rxLen = 0;
        return false;
    }
    if (rxLen < 3 + flen + 2)
        return false;
    rxLen = 0;
    return (rx[3 + flen] | (rx[4 + flen] << 8)) == crc16(&rx[3], flen);
}

void SDBulkLink::start(uint8_t op, const uint8_t *body, uint16_t len, uint8_t *resp, uint16_t *respLen,
                       uint32_t timeoutMs, int tries)
{
    if (len > maxPayload)
    {
        pendStatus = SDBulk::BAD_REQUEST;
        return;
    }

    const uint16_t flen = len + 2;
    tx[0] = SYNC;
    tx[1] = flen;
    tx[2] = flen >> 8;
    tx[3] = op;
    tx[4] = seq;
    memcpy(&tx[5], body, len);
    const uint16_t c = crc16(&tx[3], flen);
    tx[3 + flen] = c;
    tx[4 + flen] = c >> 8;
    txLen = 5 + flen;

    calls++;
    pResp = resp;
    pRespLen = respLen;
    pTimeout = timeoutMs;
    pTriesLeft = tries;
    pendStatus = SDBulk::PENDING;
    sendFrame();
}

void SDBulkLink::sendFrame()
{
    pTriesLeft--;
    rxLen = 0;
    transport->bulkSend(tx, txLen);
    pStart = millis();
}

int SDBulkLink::poll()
{
    if (pendStatus != SDBulk::PENDING)
        return pendStatus;

    uint8_t buf[128];
    lastGot = transport->bulkRead(buf, sizeof(buf));
    for (int i = 0; i < lastGot; i++)
    {
        if (parse(buf[i]) && rx[3] == (tx[3] | RESPONSE) && rx[4] == seq)
        {
            const uint16_t n = (rx[1] | (rx[2] << 8)) - 3;
            if (pResp)
                memcpy(pResp, &rx[6], n);
            if (pRespLen)
                *pRespLen = n;
            seq++;
            pendStatus = rx[5];
            return pendStatus;
        }
    }
    if (millis() - pStart >= pTimeout)
    {
        if (pTriesLeft <= 0)
        {
            pendStatus = SDBulk::NO_ANSWER;
            return pendStatus;
        }
        retries++;
        sendFrame();
    }
    return SDBulk::PENDING;
}

int SDBulkLink::call(uint8_t op, const uint8_t *body, uint16_t len, uint8_t *resp, uint16_t *respLen,
                     uint32_t timeoutMs, int tries)
{
    start(op, body, len, resp, respLen, timeoutMs, tries);
    for (;;)
    {
        const int st = poll();
        if (st != SDBulk::PENDING)
            return st;
        // never sleep mid-frame: a 512 B answer outruns a 1 ms tick at 3.75 Mbaud
        if (lastGot <= 0)
        {
            if (rxLen)
                taskYIELD();
            else
                vTaskDelay(1);
        }
    }
}

int SDBulkLink::hello(uint32_t *freeKb, uint32_t *rtc)
{
    const uint8_t b[3] = {1, (uint8_t)SDBulk::MAX_PAYLOAD, (uint8_t)(SDBulk::MAX_PAYLOAD >> 8)};
    uint8_t r[16];
    uint16_t n = 0;
    // the handset may still be finishing its last channels frame when we start
    const int st = call(SDBulk::HELLO, b, sizeof(b), r, &n, 100, 30);
    if (st == SDBulk::OK && n >= 12)
    {
        maxPayload = r[1] | (r[2] << 8);
        if (freeKb)
            memcpy(freeKb, &r[3], 4);
        if (rtc)
            memcpy(rtc, &r[8], 4);
    }
    return st;
}

int SDBulkLink::stat(const char *path, uint32_t *size, bool *isDir)
{
    uint8_t r[16];
    uint16_t n = 0;
    const int st = call(SDBulk::STAT, (const uint8_t *)path, strlen(path), r, &n);
    if (st == SDBulk::OK && n >= 9)
    {
        if (size)
            memcpy(size, r, 4);
        if (isDir)
            *isDir = r[8];
    }
    return st;
}

int SDBulkLink::mkdir(const char *path)
{
    return call(SDBulk::MKDIR, (const uint8_t *)path, strlen(path));
}

int SDBulkLink::open(const char *path, uint8_t mode, uint8_t *handle, uint32_t *size)
{
    uint8_t b[80];
    const size_t pl = strnlen(path, sizeof(b) - 1);
    b[0] = mode;
    memcpy(&b[1], path, pl);
    uint8_t r[8];
    uint16_t n = 0;
    const int st = call(SDBulk::OPEN, b, 1 + pl, r, &n);
    if (st == SDBulk::OK && n >= 5)
    {
        *handle = r[0];
        if (size)
            memcpy(size, &r[1], 4);
    }
    return st;
}

int SDBulkLink::read(uint8_t handle, uint32_t offset, uint8_t *data, uint16_t want, uint16_t *got)
{
    uint8_t b[7] = {handle};
    memcpy(&b[1], &offset, 4);
    memcpy(&b[5], &want, 2);
    *got = 0;
    // a read answers in ~3 ms; when a frame is lost (it happens while ESP-NOW is busy)
    // waiting out the full default timeout cost more than the rest of the transfer
    return call(SDBulk::READ, b, sizeof(b), data, got, READ_TIMEOUT_MS, 20);
}

void SDBulkLink::readStart(uint8_t handle, uint32_t offset, uint8_t *data, uint16_t want, uint16_t *got)
{
    uint8_t b[7] = {handle};
    memcpy(&b[1], &offset, 4);
    memcpy(&b[5], &want, 2);
    *got = 0;
    start(SDBulk::READ, b, sizeof(b), data, got, READ_TIMEOUT_MS, 20);
}

int SDBulkLink::write(uint8_t handle, uint32_t offset, const uint8_t *data, uint16_t len)
{
    static uint8_t b[SDBulk::MAX_PAYLOAD];
    if (len + 5u > maxPayload)
        return SDBulk::BAD_REQUEST;
    b[0] = handle;
    memcpy(&b[1], &offset, 4);
    memcpy(&b[5], data, len);
    uint8_t r[4];
    uint16_t n = 0;
    int st = call(SDBulk::WRITE, b, 5 + len, r, &n);
    if (st == SDBulk::OK && (n < 2 || (r[0] | (r[1] << 8)) != len))
        st = SDBulk::NO_SPACE;
    return st;
}

int SDBulkLink::close(uint8_t handle)
{
    return call(SDBulk::CLOSE, &handle, 1);
}

int SDBulkLink::rename(const char *from, const char *to)
{
    uint8_t b[140];
    const size_t fl = strnlen(from, 64), tl = strnlen(to, 64);
    b[0] = fl;
    memcpy(&b[1], from, fl);
    memcpy(&b[1 + fl], to, tl);
    return call(SDBulk::RENAME, b, 1 + fl + tl);
}

int SDBulkLink::remove(const char *path)
{
    return call(SDBulk::REMOVE, (const uint8_t *)path, strlen(path));
}

int SDBulkLink::progress(uint32_t done, uint32_t total, const char *text)
{
    uint8_t b[41] = {0};
    memcpy(&b[1], &done, 4);
    memcpy(&b[5], &total, 4);
    const size_t tl = strnlen(text, 32);
    memcpy(&b[9], text, tl);
    return call(SDBulk::PROGRESS, b, 9 + tl);
}

void SDBulkLink::end(uint8_t result, const char *text)
{
    uint8_t b[49];
    b[0] = result;
    const size_t tl = strnlen(text, 48);
    memcpy(&b[1], text, tl);
    memcpy(endText, text, tl);
    endText[tl] = 0;
    call(SDBulk::END, b, 1 + tl, nullptr, nullptr, 100, 3);
}

#endif
