#if defined(PLATFORM_ESP32) && !defined(PLATFORM_ESP32_C3)

#if defined(SCREEN_SIM)
#include "Arduino_GFX.h"
#include "canvas/Arduino_Canvas.h"
#else
#include <Arduino_GFX_Library.h>
#endif
#include <math.h>

#include "tftdisplay.h"

#include "OTA.h"
#include "common.h"
#include "logging.h"
#include "options.h"
#include "uimodel.h"
#include "qrcodegen/qrcodegen.h"

#include "WiFi.h"
extern WiFiMode_t wifiMode;

/*
 * "Black glass" instrument UI for the 160x80 ST7735.
 *
 * Near-monochrome hairline graphics with a single signal-orange accent (red
 * when armed or alarmed). Every frame is composed in an Arduino_Canvas and
 * pushed in one go, then a faint scanline pass gives it a CRT feel. Scene
 * handling mirrors the OLED renderer.
 */

#define SCREEN_X 160
#define SCREEN_Y 80

// ---------------------------------------------------------------- palette

#define C565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

static const uint16_t K_BG  = C565(0, 0, 0);
static const uint16_t K_G1  = C565(26, 26, 32);    // faint structure
static const uint16_t K_G2  = C565(60, 60, 70);    // ticks, rules
static const uint16_t K_G3  = C565(128, 128, 140); // labels
static const uint16_t K_W   = C565(238, 238, 242); // data
static const uint16_t K_ORANGE = C565(255, 96, 28);
static const uint16_t K_RED = C565(255, 36, 48);

static uint16_t mix(uint16_t a, uint16_t b, int t) // t 0..256: a -> b
{
    if (t <= 0) return a;
    if (t >= 256) return b;
    const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
    const int br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    return ((ar + ((br - ar) * t >> 8)) << 11) | ((ag + ((bg - ag) * t >> 8)) << 5) | (ab + ((bb - ab) * t >> 8));
}

static bool alarm()
{
    return ui.link == UI_ARMED || ui.link == UI_NOHANDSET || ui.link == UI_RADIOFAIL;
}

static uint16_t accent()
{
    return alarm() ? K_RED : K_ORANGE;
}

// amber phosphor: black -> ember -> orange -> hot white
static uint16_t phosphor(int v)
{
    static const uint8_t stops[][3] = {
        {0, 0, 0}, {48, 10, 0}, {130, 40, 6}, {255, 96, 28}, {255, 170, 90}, {255, 240, 220},
    };
    if (v <= 0) return K_BG;
    if (v > 255) v = 255;
    const int seg = v * 5 / 256;
    const int f = (v * 5) % 256;
    const uint8_t *a = stops[seg], *b = stops[seg + 1];
    return C565(a[0] + (b[0] - a[0]) * f / 256, a[1] + (b[1] - a[1]) * f / 256, a[2] + (b[2] - a[2]) * f / 256);
}

// ---------------------------------------------------------------- state

typedef enum {
    SC_NONE, SC_SPLASH, SC_IDLE, SC_MENU, SC_VALUE, SC_CONFIRM,
    SC_BIND, SC_WIFI, SC_BLE, SC_BUSY, SC_STATS,
} scene_t;

typedef enum { CF_BIND, CF_WIFI, CF_BLE } confirm_t;

static scene_t scene = SC_NONE;
static uint32_t sceneStart, lastFrame;

#define ANIM_MS 220
static uint32_t animStart;
static int animDir;

static int menuCur = -1;
static int valMenu, valIdx, valMin, valMax = -1, valSaved;
static confirm_t confirmKind;
static const char *busyText;
static uint8_t statsPage;

#if !defined(SCREEN_SIM)
static Arduino_DataBus *bus;
static Arduino_GFX *gfx;
#endif
static Arduino_Canvas *cv;
static uint16_t *fb;

static uint16_t *logo;
static bool logoValid;
#define LOGO_H 53

#define F_SMALL  u8g2_font_5x7_tr
#define F_SMALLF u8g2_font_5x7_tf
#define F_MED    u8g2_font_6x10_tr
#define F_BOLD   u8g2_font_helvB08_tr
#define F_BOLD12 u8g2_font_helvB12_tr
#define F_BOLD14 u8g2_font_helvB14_tr
#define F_BOLD18 u8g2_font_helvB18_tr
#define F_NUM20  u8g2_font_logisoso20_tn
#define F_NUM32  u8g2_font_logisoso32_tn

static uint32_t rng = 0x9E3779B9;
static uint32_t xrand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}

// ---------------------------------------------------------------- primitives

static int textW(const char *s)
{
    int16_t x1, y1;
    uint16_t w, h;
    cv->getTextBounds(s, 0, 40, &x1, &y1, &w, &h);
    return w;
}

static int text(int x, int y, const char *s, uint16_t c)
{
    cv->setTextColor(c);
    cv->setCursor(x, y);
    cv->print(s);
    return textW(s);
}

static void textC(int cx, int y, const char *s, uint16_t c) { text(cx - textW(s) / 2, y, s, c); }
static void textR(int rx, int y, const char *s, uint16_t c) { text(rx - textW(s), y, s, c); }

static void pixel(int x, int y, uint16_t c)
{
    if ((unsigned)x < SCREEN_X && (unsigned)y < SCREEN_Y)
        fb[y * SCREEN_X + x] = c;
}

static void blendPixel(int x, int y, uint16_t c, int a)
{
    if ((unsigned)x < SCREEN_X && (unsigned)y < SCREEN_Y)
        fb[y * SCREEN_X + x] = mix(fb[y * SCREEN_X + x], c, a);
}

static void dotsH(int x0, int x1, int y, int step, uint16_t c)
{
    for (int x = x0; x <= x1; x += step)
        pixel(x, y, c);
}

static void dotsV(int x, int y0, int y1, int step, uint16_t c)
{
    for (int y = y0; y <= y1; y += step)
        pixel(x, y, c);
}

// Viewfinder corner brackets
static void brackets(int x, int y, int w, int h, int l, uint16_t c)
{
    cv->drawFastHLine(x, y, l, c);
    cv->drawFastVLine(x, y, l, c);
    cv->drawFastHLine(x + w - l, y, l, c);
    cv->drawFastVLine(x + w - 1, y, l, c);
    cv->drawFastHLine(x, y + h - 1, l, c);
    cv->drawFastVLine(x, y + h - l, l, c);
    cv->drawFastHLine(x + w - l, y + h - 1, l, c);
    cv->drawFastVLine(x + w - 1, y + h - l, l, c);
}

// Top rule with a ruler tick every 8px
static void topRule(int y)
{
    cv->drawFastHLine(0, y, SCREEN_X, K_G2);
    for (int x = 0; x < SCREEN_X; x += 8)
        pixel(x, y + 1, (x % 32) ? K_G1 : K_G2);
}

// Segmented bar, 0..100
static void segBar(int x, int y, int w, int h, int pct, uint16_t on)
{
    const int segs = w / 4;
    const int lit = (pct * segs + 50) / 100;
    for (int i = 0; i < segs; i++)
        cv->fillRect(x + i * 4, y, 3, h, i < lit ? on : K_G1);
}

// CRT scanlines: darken every other row a touch
static void scanlines()
{
    for (int y = 1; y < SCREEN_Y; y += 2)
    {
        uint16_t *row = fb + y * SCREEN_X;
        for (int x = 0; x < SCREEN_X; x++)
        {
            const uint16_t p = row[x];
            if (p)
                row[x] = ((((p >> 11) * 7) >> 3) << 11) | (((((p >> 5) & 63) * 7) >> 3) << 5) | (((p & 31) * 7) >> 3);
        }
    }
}

static void triUp(int cx, int y, int s, uint16_t c) { cv->fillTriangle(cx - s, y + s, cx + s, y + s, cx, y, c); }
static void triDown(int cx, int y, int s, uint16_t c) { cv->fillTriangle(cx - s, y, cx + s, y, cx, y + s, c); }

// ---------------------------------------------------------------- common

static bool linkUp()
{
    return ui.link == UI_LINKED || ui.link == UI_ARMED || ui.link == UI_MISMATCH;
}

static const char *shortPower(uint8_t idx)
{
    const char *p = Display::getValue(STATE_POWER, idx);
    if (!strcmp(p, "1000mW")) return "1W";
    if (!strcmp(p, "2000mW")) return "2W";
    return p;
}

static void fmtTimer(char *buf, size_t n, uint32_t ms)
{
    const uint32_t s = ms / 1000;
    snprintf(buf, n, "%02u:%02u", (unsigned)(s / 60) % 100, (unsigned)(s % 60));
}

static const char *radioName()
{
#if defined(RADIO_LR1121)
    return "LR1121";
#elif defined(RADIO_LR2021)
    return "LR2021";
#elif defined(RADIO_SX127X)
    return "SX127X";
#else
    return "SX1280";
#endif
}

static void statusBar(uint32_t now)
{
    const bool blink = (now / 400) & 1;
    const uint16_t a = accent();
    const char *label = "";
    bool dotOn = true;
    switch (ui.link)
    {
    case UI_SEARCHING: label = "ACQUIRING"; dotOn = blink; break;
    case UI_LINKED: label = "LINK"; break;
    case UI_ARMED: label = "ARMED"; break;
    case UI_MISMATCH: label = "MODEL MISMATCH"; dotOn = blink; break;
    case UI_NOHANDSET: label = "NO HANDSET"; dotOn = blink; break;
    case UI_RADIOFAIL: label = "RADIO FAIL"; dotOn = blink; break;
    }
    if (dotOn)
        cv->fillCircle(3, 4, 2, ui.link == UI_LINKED ? K_W : a);
    else
        cv->drawCircle(3, 4, 2, K_G2);
    cv->setFont(F_SMALL);
    text(9, 8, label, ui.link == UI_LINKED ? K_W : a);

    char buf[16];
    if (ui.link == UI_ARMED && ui.armed_at)
    {
        fmtTimer(buf, sizeof(buf), now - ui.armed_at);
        cv->setFont(F_SMALL);
        const int w = textW(buf);
        textR(159, 8, buf, K_W);
        text(159 - w - 12, 8, "T+", a);
    }
    else if (ui.temperature)
    {
        cv->setFont(F_SMALLF);
        snprintf(buf, sizeof(buf), "%d\xb0" "C", ui.temperature);
        textR(159, 8, buf, ui.temperature > 60 ? a : K_G3);
    }
    topRule(10);
}

// ---------------------------------------------------------------- splash

static void drawSplash(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    const uint16_t a = K_ORANGE;
    if (t < 1100)
    {
        // vendor logo in, then out
        const int al = t < 350 ? t * 256 / 350 : (t > 850 ? (1100 - t) * 256 / 250 : 256);
        if (logoValid)
            for (int i = 0; i < SCREEN_X * LOGO_H; i++)
                fb[i + 13 * SCREEN_X] = mix(K_BG, logo[i], al);
        return;
    }

    // Power-on self test, typed out like a terminal
    char lines[6][32];
    char pa[24];
    snprintf(pa, sizeof(pa), "%s-%s", shortPower(ui.power_min), shortPower(ui.power_max));
    snprintf(lines[0], 32, "EXPRESSLRS %s", uiVersion());
    snprintf(lines[1], 32, "RADIO  %s", radioName());
    snprintf(lines[2], 32, "HOPS   %d CH", ui.hop_channels);
    snprintf(lines[3], 32, "PA     %s", pa);
    snprintf(lines[4], 32, "SCREEN 160X80 RGB");
    snprintf(lines[5], 32, "READY");
    const bool ok[] = {false, true, true, true, true, false};

    int budget = (t - 1100) / 14; // characters revealed so far
    cv->setFont(F_SMALL);
    int y = 9;
    for (int i = 0; i < 6 && budget > 0; i++, y += 11)
    {
        const int len = strlen(lines[i]);
        char part[32];
        const int n = budget < len ? budget : len;
        memcpy(part, lines[i], n);
        part[n] = 0;
        const int w = text(2, y, part, i == 0 || i == 5 ? K_W : K_G3);
        budget -= len + 4; // a beat between lines
        if (n < len || budget <= 0)
        {
            if ((now / 150) & 1)
                cv->fillRect(3 + w, y - 6, 4, 7, a);
            break;
        }
        if (ok[i])
        {
            dotsH(4 + w, 136, y - 1, 3, K_G2);
            text(141, y, "OK", a);
        }
        if (i == 5 && (now / 300) & 1)
            cv->fillRect(4 + w, y - 6, 4, 7, a);
    }
    // progress rule
    const int w = (t - 1100) * SCREEN_X / 1900;
    cv->drawFastHLine(0, 78, SCREEN_X, K_G1);
    cv->drawFastHLine(0, 78, w > SCREEN_X ? SCREEN_X : w, a);
}

// ---------------------------------------------------------------- idle

static void rssiTrace(int x0, int y0, int w, int h)
{
    const int n = uiHist.count < w ? uiHist.count : w;
    int prev = -1;
    for (int i = 0; i < n; i++)
    {
        const uint16_t idx = uiHistIdx(uiHist.count - n + i);
        const int8_t r = uiHist.rssi1[idx];
        const int x = x0 + w - n + i;
        if (!r) { prev = -1; continue; }
        int v = (r + 110) * (h - 1) / 70;
        if (v < 0) v = 0;
        if (v > h - 1) v = h - 1;
        const int y = y0 + h - 1 - v;
        // phosphor persistence: older samples fade toward the left
        const uint16_t c = mix(K_G1, K_W, 40 + i * 216 / (n ? n : 1));
        if (prev >= 0)
            cv->drawLine(x - 1, prev, x, y, c);
        blendPixel(x, y + 1, c, 60); // glow
        prev = y;
        const uint8_t lq = uiHist.lq[idx];
        if (lq < 90)
            pixel(x, y0 + h - 1, accent());
    }
    if (prev >= 0)
        cv->fillCircle(x0 + w - 1, prev, 1, accent());
}

static void staticNoise(int x0, int y0, int w, int h)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
        {
            const uint32_t r = xrand();
            if ((r & 3) == 0)
                pixel(x, y, mix(K_G1, K_G3, (r >> 8) & 255));
        }
}

static void spec(int y, const char *label, uint16_t lc)
{
    cv->setFont(F_SMALL);
    text(72, y, label, lc);
}

static void drawIdle(uint32_t now)
{
    statusBar(now);
    const uint16_t a = accent();
    char buf[16];

    if (ui.link == UI_NOHANDSET || ui.link == UI_RADIOFAIL)
    {
        const bool on = (now / 450) & 1;
        cv->drawTriangle(28, 20, 6, 60, 50, 60, on ? a : K_G2);
        cv->drawTriangle(28, 24, 10, 58, 46, 58, K_G1);
        cv->fillRect(27, 33, 3, 14, on ? K_W : K_G2);
        cv->fillRect(27, 50, 3, 3, on ? K_W : K_G2);
        cv->setFont(F_BOLD14);
        text(60, 38, ui.link == UI_NOHANDSET ? "NO" : "RADIO", K_W);
        text(60, 56, ui.link == UI_NOHANDSET ? "HANDSET" : "FAILURE", K_W);
        cv->setFont(F_SMALL);
        text(60, 70, ui.link == UI_NOHANDSET ? "check radio / bay" : "power cycle the TX", K_G3);
        return;
    }

    // ---- LQ block
    cv->setFont(F_SMALL);
    text(1, 21, "LQ", K_G3);
    cv->setFont(F_NUM32);
    if (linkUp())
    {
        snprintf(buf, sizeof(buf), "%d", ui.lq_up);
        textR(63, 54, buf, ui.lq_up < 50 ? a : K_W);
        segBar(1, 57, 64, 3, ui.lq_up, ui.lq_up < 50 ? a : K_W);
    }
    else
    {
        textR(63, 54, "--", K_G1);
        // scanning cursor running along the empty bar
        segBar(1, 57, 64, 3, 0, K_W);
        const int p = (now / 60) % 16;
        cv->fillRect(1 + p * 4, 57, 3, 3, a);
    }
    dotsV(67, 14, 60, 2, K_G2);

    // ---- spec rows
    char pre[8], num[8], suf[8];
    uiSplitRate(Display::getValue(STATE_PACKET, ui.rate), pre, num, suf);
    spec(23, "RATE", K_G3);
    cv->setFont(F_SMALL);
    int x = 159 - textW(suf);
    text(x, 23, suf, K_G3);
    cv->setFont(F_BOLD12);
    x -= textW(num) + 2;
    text(x, 23, num, K_W);
    if (pre[0])
    {
        cv->setFont(F_BOLD);
        x -= textW(pre) + 2;
        text(x, 23, pre, a);
    }
    dotsH(72, 159, 28, 2, K_G1);

    spec(40, ui.dynamic ? "PWR" : "POWER", K_G3);
    if (ui.dynamic)
    {
        const bool boost = ui.run_power != ui.power && ((now / 300) & 1);
        cv->setFont(F_SMALL);
        text(89, 40, "DYN", boost ? K_W : a);
    }
    cv->setFont(F_BOLD12);
    textR(159, 40, shortPower(ui.run_power), K_W);
    {
        // power ladder under the value
        const int n = ui.power_max - ui.power_min + 1;
        for (int i = 0; i < n; i++)
        {
            const int lvl = ui.power_min + i;
            const uint16_t c = lvl <= ui.run_power ? K_W : (lvl <= ui.power ? K_G2 : K_G1);
            cv->fillRect(72 + i * 5, 43, 4, 2, c);
        }
    }
    dotsH(72, 159, 47, 2, K_G1);

    spec(58, "TLM", K_G3);
    cv->setFont(F_BOLD12);
    textR(159, 58, Display::getValue(STATE_TELEMETRY_CURR, ui.tlm), K_W);

    // ---- bottom strip
    cv->drawFastHLine(0, 62, SCREEN_X, K_G2);
    if (linkUp())
    {
        rssiTrace(0, 64, SCREEN_X, 16);
        cv->setFont(F_SMALL);
        snprintf(buf, sizeof(buf), "%ddBm", ui.rssi_up1);
        const int w = textW(buf);
        cv->fillRect(0, 64, w + 3, 9, K_BG);
        text(1, 71, buf, K_G3);
    }
    else
    {
        staticNoise(0, 64, SCREEN_X, 16);
        cv->setFont(F_SMALL);
        cv->fillRect(52, 67, 56, 10, K_BG);
        textC(80, 75, "NO SIGNAL", ((now / 500) & 1) ? K_W : K_G2);
    }

    if (ui.link == UI_ARMED)
        brackets(0, 0, SCREEN_X, SCREEN_Y, 6, ((now / 500) & 1) ? a : mix(K_BG, a, 120));
}

// ---------------------------------------------------------------- menu

static const char *menuNow(int m)
{
    switch (m)
    {
    case STATE_PACKET: return Display::getValue(STATE_PACKET, ui.rate);
    case STATE_POWER: return shortPower(ui.power);
    case STATE_TELEMETRY: return Display::getValue(STATE_TELEMETRY_CURR, ui.tlm);
    default: return nullptr;
    }
}

static void drawMenu(uint32_t now)
{
    int items[24], pos = 0;
    const int n = uiMenuSiblings(items, 24, &pos);
    const float t = uiEase((now - animStart) / (float)ANIM_MS);
    const int slide = (int)((1.0f - t) * 26) * animDir;
    const uint16_t a = accent();
    char buf[24];

    // Tuning dial on the left: a long tick per item, short ticks between
    for (int y = -28; y < 108; y += 4)
    {
        const int yy = y + slide % 4;
        pixel(2, yy, K_G1), pixel(3, yy, K_G1);
    }
    for (int k = -3; k <= 3; k++)
    {
        const int i = pos + k;
        if (i < 0 || i >= n) continue;
        const int y = 40 + k * 26 + slide;
        cv->drawFastHLine(1, y, k == 0 ? 10 : 7, k == 0 ? K_W : K_G2);
    }
    cv->fillTriangle(16, 36, 16, 44, 12, 40, a);

    // Neighbours
    cv->setFont(F_SMALL);
    for (int k = -2; k <= 2; k++)
    {
        if (k == 0) continue;
        const int i = pos + k;
        if (i < 0 || i >= n) continue;
        snprintf(buf, sizeof(buf), "%s %s", Display::menuString(items[i], 0), Display::menuString(items[i], 1));
        const int y = 40 + k * 26 + slide + 3;
        text(22, y, buf, abs(k) == 1 ? K_G2 : K_G1);
    }

    // Selected item
    const int y = 40 + slide;
    snprintf(buf, sizeof(buf), "%02d", pos + 1);
    cv->setFont(F_SMALL);
    text(22, y - 10, buf, a);
    snprintf(buf, sizeof(buf), "/%02d", n);
    text(32, y - 10, buf, K_G2);
    snprintf(buf, sizeof(buf), "%s %s", Display::menuString(menuCur, 0), Display::menuString(menuCur, 1));
    cv->setFont(F_BOLD14);
    if (textW(buf) > 134)
        cv->setFont(F_BOLD12);
    text(22, y + 7, buf, K_W);
    const char *v = menuNow(menuCur);
    cv->setFont(F_SMALL);
    if (v)
    {
        text(22, y + 17, "NOW", K_G2);
        text(42, y + 17, v, K_G3);
    }
    brackets(19, y - 19, 141, 41, 4, K_G2);
}

// ---------------------------------------------------------------- value

static int valueTextW(const char *s)
{
    char buf[24];
    int w = 0, n = 0;
    for (const char *p = s; *p; p++)
    {
        if (p[0] == '!' && (p[1] == '+' || p[1] == '-')) { w += 10; p++; continue; }
        buf[n++] = *p;
    }
    buf[n] = 0;
    return w + textW(buf);
}

static void drawValueText(int cx, int y, const char *s, uint16_t c)
{
    // "!+" / "!-" become small triangles
    char buf[24];
    int x = cx - valueTextW(s) / 2;
    for (const char *p = s; *p;)
    {
        int n = 0;
        while (*p && !(p[0] == '!' && (p[1] == '+' || p[1] == '-')))
            buf[n++] = *p++;
        buf[n] = 0;
        if (n)
            x += text(x, y, buf, c);
        if (*p)
        {
            if (p[1] == '+') triUp(x + 5, y - 9, 4, c);
            else triDown(x + 5, y - 8, 4, c);
            x += 10;
            p += 2;
        }
    }
}

static void drawValue(uint32_t now)
{
    char buf[24];
    const uint16_t a = accent();
    cv->setFont(F_SMALL);
    snprintf(buf, sizeof(buf), "%s %s", Display::menuString(valMenu, 0), Display::menuString(valMenu, 1));
    text(2, 8, buf, K_G3);
    if (valMax >= valMin)
    {
        snprintf(buf, sizeof(buf), "/%02d", valMax - valMin + 1);
        const int w = textW(buf);
        textR(159, 8, buf, K_G2);
        snprintf(buf, sizeof(buf), "%02d", valIdx - valMin + 1);
        textR(159 - w, 8, buf, a);
    }
    topRule(10);

    const float t = uiEase((now - animStart) / (float)ANIM_MS);
    const int slide = (int)((1.0f - t) * 70) * animDir;

    // carousel: neighbours sit 70px either side and slide through
    for (int k = -2; k <= 2; k++)
    {
        const int idx = valIdx + k;
        if (idx < valMin || idx > valMax) continue;
        const char *v = Display::getValue(valMenu, idx);
        if (!v) continue;
        const int cx = 80 + k * 70 + slide;
        if (cx < -60 || cx > 220) continue;
        const int d = abs(cx - 80);
        if (d < 30)
        {
            cv->setFont(F_BOLD14);
            drawValueText(cx, 40, v, k == 0 ? K_W : mix(K_W, K_G2, d * 8));
        }
        else
        {
            cv->setFont(F_MED);
            drawValueText(cx, 38, v, mix(K_G3, K_G1, (d - 30) * 4));
        }
    }

    // tape ruler: one tick per option, current under the caret
    const int step = 9;
    const int base = 56;
    const int off = (int)((1.0f - t) * step) * animDir;
    for (int i = valMin; i <= valMax; i++)
    {
        const int x = 80 + (i - valIdx) * step + off;
        if (x < 0 || x >= SCREEN_X) continue;
        const bool cur = i == valIdx;
        cv->drawFastVLine(x, base - (cur ? 6 : 3), cur ? 6 : 3, cur ? a : K_G2);
        if (i == valSaved)
            pixel(x, base - 9, K_W), pixel(x + 1, base - 9, K_W);
    }
    cv->drawFastHLine(0, base, SCREEN_X, K_G2);
    cv->fillTriangle(80, base + 2, 76, base + 7, 84, base + 7, a);

    cv->setFont(F_SMALL);
    text(2, 77, "HOLD < BACK", K_G2);
    if (valIdx == valSaved)
    {
        cv->fillCircle(116, 74, 2, a);
        textR(159, 77, "SAVED", K_W);
    }
    else
    {
        textR(159, 77, "OK TO SAVE", K_W);
    }
}

// ---------------------------------------------------------------- confirm / status

// small vector glyphs drawn in hairline
static void glyphBind(int cx, int cy, uint32_t t)
{
    const int o = (int)(sinf(t / 300.0f) * 2);
    cv->drawCircle(cx - 8 - o, cy, 11, K_W);
    cv->drawCircle(cx + 8 + o, cy, 11, K_W);
    cv->drawCircle(cx - 8 - o, cy, 10, K_G2);
    cv->drawCircle(cx + 8 + o, cy, 10, K_G2);
}

static void glyphWifi(int cx, int cy, uint32_t t)
{
    const int lit = (t / 250) % 4;
    for (int i = 0; i < 3; i++)
        cv->drawArc(cx, cy, 8 + i * 8, 7 + i * 8, 225, 315, i < lit ? K_W : K_G2);
    cv->fillCircle(cx, cy, 2, accent());
}

static void glyphStick(int cx, int cy, uint32_t t)
{
    cv->drawRoundRect(cx - 16, cy - 16, 33, 33, 6, K_G2);
    dotsH(cx - 14, cx + 14, cy, 3, K_G1);
    dotsV(cx, cy - 14, cy + 14, 3, K_G1);
    const float s = t / 1000.0f;
    const int x = cx + (int)(sinf(s * 2.1f) * 11), y = cy + (int)(sinf(s * 3.3f) * 11);
    cv->drawLine(cx, cy, x, y, K_G3);
    cv->fillCircle(x, y, 3, accent());
}

static void drawConfirm(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    const uint16_t a = accent();
    const char *title = "", *sub = "";
    switch (confirmKind)
    {
    case CF_BIND: title = "BIND"; sub = "send a bind request"; glyphBind(130, 30, t); break;
    case CF_WIFI: title = "WIFI"; sub = "web update mode"; glyphWifi(130, 40, t); break;
    case CF_BLE: title = "GAMEPAD"; sub = "bluetooth joystick"; glyphStick(136, 30, t); break;
    }
    brackets(0, 0, SCREEN_X, SCREEN_Y, 6, K_G2);
    cv->setFont(F_BOLD18);
    if (textW(title) > 100)
        cv->setFont(F_BOLD14);
    text(8, 28, title, K_W);
    cv->setFont(F_SMALL);
    text(8, 40, sub, K_G3);
    // button
    const int pulse = 128 + (int)(sinf(t / 200.0f) * 127);
    cv->fillRect(8, 54, 64, 16, mix(K_BG, a, pulse / 3));
    cv->drawRect(8, 54, 64, 16, a);
    cv->setFont(F_BOLD);
    textC(40, 66, "PRESS OK", K_W);
    cv->setFont(F_SMALL);
    text(80, 66, "< BACK", K_G2);
}

static void drawBind(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    const int cx = 120, cy = 42;
    for (int k = 0; k < 4; k++)
    {
        const int r = ((t / 24) + k * 11) % 44;
        cv->drawCircle(cx, cy, r, mix(K_W, K_G1, r * 256 / 44));
    }
    cv->fillCircle(cx, cy, 3, accent());
    cv->setFont(F_BOLD14);
    text(6, 28, "BINDING", K_W);
    char buf[16];
    snprintf(buf, sizeof(buf), "T+%02u", (unsigned)(t / 1000));
    cv->setFont(F_SMALL);
    text(6, 40, buf, accent());
    text(6, 62, "put the RX in", K_G3);
    text(6, 71, "bind mode", K_G3);
}

static void drawBusy(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    const float head = (t % 1200) * 2 * M_PI / 1200;
    for (int k = 0; k < 24; k++)
    {
        const float ang = k * 2 * M_PI / 24;
        float d = head - ang;
        while (d < 0) d += 2 * M_PI;
        const int b = 256 - (int)(d * 256 / (2 * M_PI));
        const int x0 = 130 + (int)(cosf(ang) * 12), y0 = 40 + (int)(sinf(ang) * 12);
        const int x1 = 130 + (int)(cosf(ang) * 20), y1 = 40 + (int)(sinf(ang) * 20);
        cv->drawLine(x0, y0, x1, y1, b > 230 ? accent() : mix(K_G1, K_W, b));
    }
    cv->setFont(F_BOLD14);
    text(6, 46, busyText, K_W);
}

static void drawBLE(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    glyphStick(128, 42, t);
    cv->setFont(F_BOLD14);
    text(6, 28, "GAMEPAD", K_W);
    cv->setFont(F_SMALL);
    text(6, 40, "bluetooth joystick", K_G3);
    if ((now / 500) & 1)
        cv->fillCircle(8, 52, 2, accent());
    text(14, 55, "LIVE", K_W);
}

static void drawQR(const char *s, int x, int y, int box)
{
    static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(5)];
    if (!qrcodegen_encodeText(s, tmp, qr, qrcodegen_Ecc_LOW, 1, 5, qrcodegen_Mask_AUTO, true))
        return;
    const int size = qrcodegen_getSize(qr);
    const int scale = (size + 2) * 2 <= box ? 2 : 1;
    const int ox = x + (box - size * scale) / 2, oy = y + (box - size * scale) / 2;
    cv->fillRect(ox - 3, oy - 3, size * scale + 6, size * scale + 6, K_W);
    for (int my = 0; my < size; my++)
        for (int mx = 0; mx < size; mx++)
            if (qrcodegen_getModule(qr, mx, my))
                cv->fillRect(ox + mx * scale, oy + my * scale, scale, scale, K_BG);
}

static void kv(int x, int y, const char *k, const char *v)
{
    cv->setFont(F_SMALL);
    text(x, y - 10, k, K_G3);
    cv->setFont(F_MED);
    if (textW(v) > SCREEN_X - x)
        cv->setFont(F_SMALL);
    text(x, y, v, K_W);
}

static void drawWiFi(uint32_t now)
{
    char buf[96];
    if (wifiMode == WIFI_STA)
    {
        snprintf(buf, sizeof(buf), "http://%s.local", wifi_hostname);
        drawQR(buf, 2, 2, 76);
        snprintf(buf, sizeof(buf), "%s.local", wifi_hostname);
        kv(84, 22, "OPEN IN BROWSER", buf);
        kv(84, 48, "OR SCAN", "the code");
    }
    else
    {
        snprintf(buf, sizeof(buf), "WIFI:T:WPA;S:%s;P:%s;;", wifi_ap_ssid, wifi_ap_password);
        drawQR(buf, 2, 2, 76);
        kv(84, 20, "SSID", wifi_ap_ssid);
        kv(84, 45, "PASSWORD", wifi_ap_password);
        kv(84, 70, "THEN OPEN", wifi_ap_address);
    }
}

// ---------------------------------------------------------------- link stats

static void statsHeader(const char *title)
{
    cv->fillRect(1, 2, 4, 5, accent());
    cv->setFont(F_SMALL);
    text(8, 8, title, K_W);
    for (int i = 0; i < 4; i++)
        cv->fillRect(140 + i * 5, 3, 4, 3, i == statsPage ? K_W : K_G1);
    topRule(10);
}

static void drawStatsLink(uint32_t now)
{
    statsHeader("LINK");
    char buf[20];
    const bool up = linkUp();
    const uint16_t a = accent();
    for (int side = 0; side < 2; side++)
    {
        const int x = side ? 84 : 2;
        const int lq = up ? (side ? ui.lq_dn : ui.lq_up) : 0;
        const int8_t r1 = side ? ui.rssi_dn1 : ui.rssi_up1;
        const int8_t r2 = side ? ui.rssi_dn2 : ui.rssi_up2;
        const int8_t snr = side ? ui.snr_dn : ui.snr_up;
        cv->setFont(F_SMALL);
        text(x, 21, side ? "DOWNLINK" : "UPLINK", K_G3);
        cv->setFont(F_NUM20);
        if (up)
            snprintf(buf, sizeof(buf), "%d", lq);
        else
            snprintf(buf, sizeof(buf), "--");
        text(x, 45, buf, up && lq < 50 ? a : K_W);
        cv->setFont(F_SMALL);
        text(x + 44, 45, "LQ", K_G2);
        segBar(x, 49, 72, 2, lq, lq < 50 ? a : K_W);
        if (up && r2)
            snprintf(buf, sizeof(buf), "%d/%d", r1, r2);
        else if (up)
            snprintf(buf, sizeof(buf), "%d", r1);
        else
            snprintf(buf, sizeof(buf), "--");
        text(x, 63, "RSSI", K_G2);
        textR(x + 72, 63, buf, K_W);
        snprintf(buf, sizeof(buf), "%d", snr);
        text(x, 73, "SNR", K_G2);
        textR(x + 72, 73, up ? buf : "--", K_W);
    }
    dotsV(80, 14, 78, 2, K_G2);
    cv->setFont(F_SMALL);
    snprintf(buf, sizeof(buf), "ANT %d", ui.ant + 1);
    textR(132, 8, buf, a);
}

static int rssiY(int8_t r, int y0, int h)
{
    int v = (-40 - r) * (h - 1) / 80;
    if (v < 0) v = 0;
    if (v > h - 1) v = h - 1;
    return y0 + v;
}

static void drawStatsScope(uint32_t now)
{
    statsHeader("RSSI SCOPE");
    char buf[16];
    const uint16_t a = accent();
    const int x0 = 0, y0 = 13, w = 160, h = 66;
    // graticule: 8 x 4 divisions
    for (int i = 1; i < 8; i++)
        dotsV(x0 + i * w / 8, y0, y0 + h - 1, 3, K_G1);
    for (int g = 0; g <= 4; g++)
    {
        const int y = rssiY(-40 - g * 20, y0, h);
        dotsH(x0, x0 + w - 1, y, 3, g == 2 ? K_G2 : K_G1);
    }
    cv->setFont(F_SMALL);
    text(2, rssiY(-60, y0, h) - 2, "-60", K_G2);
    text(2, rssiY(-100, y0, h) - 2, "-100", K_G2);

    const int n = uiHist.count < w ? uiHist.count : w;
    int p1 = -1, p2 = -1;
    for (int i = 0; i < n; i++)
    {
        const uint16_t idx = uiHistIdx(uiHist.count - n + i);
        const int x = x0 + w - n + i;
        const int age = i * 256 / (n ? n : 1);
        const int8_t r1 = uiHist.rssi1[idx], r2 = uiHist.rssi2[idx], rd = uiHist.rssi_dn[idx];
        if (rd && (i & 1))
            pixel(x, rssiY(rd, y0, h), mix(K_BG, a, 60 + age * 3 / 4));
        if (r2)
        {
            const int y = rssiY(r2, y0, h);
            if (p2 >= 0) cv->drawLine(x - 1, p2, x, y, mix(K_G1, K_G3, age));
            p2 = y;
        }
        else p2 = -1;
        if (r1)
        {
            const int y = rssiY(r1, y0, h);
            const uint16_t c = mix(K_G1, K_W, 30 + age * 226 / 256);
            if (p1 >= 0) cv->drawLine(x - 1, p1, x, y, c);
            blendPixel(x, y - 1, c, 50);
            blendPixel(x, y + 1, c, 50);
            p1 = y;
        }
        else p1 = -1;
    }
    if (linkUp())
    {
        cv->setFont(F_SMALL);
        snprintf(buf, sizeof(buf), "%d", ui.rssi_up1);
        textR(132, 8, buf, K_W);
        snprintf(buf, sizeof(buf), "%d", ui.rssi_dn1);
        textR(110, 8, buf, a);
    }
}

// Polar "sunburst": one spoke per FHSS channel, length = telemetry success
static void drawStatsHops(uint32_t now)
{
    const int n = ui.hop_channels ? ui.hop_channels : 80;
    char buf[24];
    const uint16_t a = accent();
    statsHeader("HOPS");
    uint32_t okSum = 0, cnt = 0, bad = 0;
    for (int ch = 0; ch < n; ch++)
        if (hopStats[ch].hits || hopStats[ch].ok)
        {
            okSum += hopStats[ch].ok, cnt++;
            if (hopStats[ch].ok < 140) bad++;
        }

    const int cx = 80, cy = 45, rIn = 9, rOut = 30;
    cv->drawCircle(cx, cy, rOut + 1, K_G1);
    cv->drawCircle(cx, cy, (rIn + rOut) / 2, K_G1);
    for (int ch = 0; ch < n; ch++)
    {
        const float ang = ch * 2 * M_PI / n - M_PI / 2;
        const float c = cosf(ang), s = sinf(ang);
        const hopstat_t &st = hopStats[ch];
        if (!st.hits && !st.ok)
        {
            pixel(cx + (int)(c * rOut), cy + (int)(s * rOut), K_G2);
            continue;
        }
        const int len = rIn + st.ok * (rOut - rIn) / 255;
        const uint16_t col = st.ok < 140 ? a : mix(K_G2, K_W, (st.ok - 140) * 256 / 115);
        cv->drawLine(cx + (int)(c * rIn), cy + (int)(s * rIn), cx + (int)(c * len), cy + (int)(s * len), col);
        // RSSI as a dot ring just outside
        const int rl = uiRssiLevel(st.rssi);
        if (rl)
            pixel(cx + (int)(c * (rOut + 3)), cy + (int)(s * (rOut + 3)), mix(K_BG, K_G3, rl));
    }
    {
        // current hop
        const float ang = hopStatsCurrent * 2 * M_PI / n - M_PI / 2;
        cv->drawLine(cx + (int)(cosf(ang) * (rOut + 2)), cy + (int)(sinf(ang) * (rOut + 2)),
                     cx + (int)(cosf(ang) * (rOut + 6)), cy + (int)(sinf(ang) * (rOut + 6)), a);
    }
    cv->fillCircle(cx, cy, 2, K_G3);

    // side readouts
    cv->setFont(F_SMALL);
    text(2, 24, "TLM OK", K_G3);
    cv->setFont(F_NUM20);
    if (cnt)
        snprintf(buf, sizeof(buf), "%d", (int)(okSum * 100 / 255 / cnt));
    else
        snprintf(buf, sizeof(buf), "--");
    text(2, 48, buf, K_W);
    cv->setFont(F_SMALL);
    text(2, 58, "%", K_G2);
    textR(159, 24, "CHANNELS", K_G3);
    snprintf(buf, sizeof(buf), "%d", n);
    cv->setFont(F_BOLD12);
    textR(159, 40, buf, K_W);
    cv->setFont(F_SMALL);
    textR(159, 58, "WEAK", K_G3);
    snprintf(buf, sizeof(buf), "%d", (int)bad);
    cv->setFont(F_BOLD12);
    textR(159, 74, buf, bad ? a : K_W);
}

static void drawStatsWaterfall(uint32_t now)
{
    statsHeader("WATERFALL");
    int lo = 255, hi = 0;
    for (int r = 0; r < UI_WF_ROWS; r++)
        for (int c = 0; c < UI_WF_COLS; c++)
        {
            const uint8_t v = uiWaterfall[r][c];
            if (!v) continue;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    if (hi <= lo) hi = lo + 1;
    const int y0 = 13;
    for (int r = 0; r < UI_WF_ROWS; r++)
    {
        const uint8_t *row = uiWaterfall[(uiWfHead + UI_WF_ROWS - r) % UI_WF_ROWS];
        for (int x = 0; x < SCREEN_X; x++)
        {
            const uint8_t v = row[x * UI_WF_COLS / SCREEN_X];
            pixel(x, y0 + UI_WF_ROWS - 1 - r, v ? phosphor(20 + (v - lo) * 235 / (hi - lo)) : K_BG); // newest at the bottom
        }
    }
    const int yb = y0 + UI_WF_ROWS + 1;
    cv->drawFastHLine(0, yb, SCREEN_X, K_G2);
    for (int x = 0; x < SCREEN_X; x += 16)
        cv->drawFastVLine(x, yb + 1, 2, K_G2);
    const int n = ui.hop_channels ? ui.hop_channels : 80;
    const int cx = hopStatsCurrent * SCREEN_X / n;
    cv->fillTriangle(cx, yb + 1, cx - 3, yb + 5, cx + 3, yb + 5, accent());
    cv->setFont(F_SMALL);
    text(0, 79, "CH0", K_G3);
    char buf[8];
    snprintf(buf, sizeof(buf), "CH%d", n - 1);
    textR(159, 79, buf, K_G3);
    textC(80, 79, "24 SEC", K_G2);
}

static void drawStats(uint32_t now)
{
    switch (statsPage)
    {
    case 0: drawStatsLink(now); break;
    case 1: drawStatsScope(now); break;
    case 2: drawStatsHops(now); break;
    default: drawStatsWaterfall(now); break;
    }
}

// ---------------------------------------------------------------- frame loop

static bool dirty;

static bool frozen(uint32_t now)
{
    return (int32_t)(uiFreezeUntil - now) > 0;
}

static void render(uint32_t now)
{
    if (frozen(now))
    {
        dirty = true; // a remote screenshot is holding the frame; draw once it lets go
        return;
    }
    dirty = false;
    cv->fillScreen(K_BG);
    cv->setTextWrap(false);
    switch (scene)
    {
    case SC_SPLASH: drawSplash(now); break;
    case SC_IDLE: drawIdle(now); break;
    case SC_MENU: drawMenu(now); break;
    case SC_VALUE: drawValue(now); break;
    case SC_CONFIRM: drawConfirm(now); break;
    case SC_BIND: drawBind(now); break;
    case SC_WIFI: drawWiFi(now); break;
    case SC_BLE: drawBLE(now); break;
    case SC_BUSY: drawBusy(now); break;
    case SC_STATS: drawStats(now); break;
    default: break;
    }
    if (scene != SC_WIFI)
        scanlines();
#if !defined(SCREEN_SIM)
    cv->flush();
#endif
    lastFrame = now;
}

static void setScene(scene_t s)
{
    if (scene != s)
        sceneStart = millis();
    scene = s;
}

static uint32_t frameInterval(uint32_t now)
{
    const bool armed = ui.link == UI_ARMED;
    if (now - animStart < ANIM_MS + 40)
        return 20;
    switch (scene)
    {
    case SC_SPLASH: return 25;
    case SC_IDLE: return armed ? 250 : 40;
    case SC_STATS: return armed ? 250 : 80;
    case SC_CONFIRM:
    case SC_BIND:
    case SC_BLE:
    case SC_BUSY: return 30;
    case SC_WIFI: return 500;
    default: return 0;
    }
}

void TFTDisplay::tick(uint32_t now)
{
    uiModelUpdate(now);
    if (frozen(now))
        return;
    if (animStart && now - animStart >= ANIM_MS)
    {
        // always land the settled frame of an animation, however late
        animStart = 0;
        render(now);
        return;
    }
    const uint32_t iv = frameInterval(now);
    if (dirty || (iv && now - lastFrame >= iv))
        render(now);
}

// ---------------------------------------------------------------- Display API

void TFTDisplay::init()
{
#if defined(SCREEN_SIM)
    cv = new Arduino_Canvas(SCREEN_X, SCREEN_Y, nullptr);
    cv->begin(GFX_SKIP_OUTPUT_BEGIN);
#else
    if (GPIO_PIN_SCREEN_BL != UNDEF_PIN)
    {
        pinMode(GPIO_PIN_SCREEN_BL, OUTPUT);
    }
    bus = new Arduino_ESP32SPI(GPIO_PIN_SCREEN_DC, GPIO_PIN_SCREEN_CS, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_MOSI, GFX_NOT_DEFINED, HSPI);
    gfx = new Arduino_ST7735(bus, GPIO_PIN_SCREEN_RST, OPT_SCREEN_REVERSED ? 3 : 1 /* rotation */, true , 80, 160, 26, 1, 26, 1);
    cv = new Arduino_Canvas(SCREEN_X, SCREEN_Y, gfx);
    cv->begin();
#endif
    fb = cv->getFramebuffer();
    cv->fillScreen(K_BG);
#if !defined(SCREEN_SIM)
    cv->flush();
#endif
    doScreenBackLight(SCREEN_BACKLIGHT_ON);
}

void TFTDisplay::doScreenBackLight(screen_backlight_t state)
{
    if (GPIO_PIN_SCREEN_BL != UNDEF_PIN)
    {
        digitalWrite(GPIO_PIN_SCREEN_BL, state);
    }
}

void TFTDisplay::printScreenshot()
{
    DBGLN("Unimplemented");
}

void TFTDisplay::dumpFrame(Print &out, int first, int count)
{
    dumpRows(out, "SCR 160 80 16", (const uint8_t *)fb, 80, SCREEN_X * SCREEN_Y * 2 / 80, first, count);
}

void TFTDisplay::displaySplashScreen()
{
    if (!logo)
        logo = (uint16_t *)malloc(SCREEN_X * LOGO_H * 2);
    logoValid = logo && spi_flash_read(logo_image, logo, SCREEN_X * LOGO_H * 2) == ESP_OK;
    setScene(SC_SPLASH);
    sceneStart = millis();
    render(millis());
}

void TFTDisplay::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    storeIdle(rate_index, power_index, ratio_index, dynamic, running_power_index, temperature);
    if (logo && scene != SC_SPLASH)
    {
        free(logo); // only needed for the splash
        logo = nullptr;
    }
    const bool fresh = scene != SC_IDLE;
    setScene(SC_IDLE);
    if (fresh || changed)
        render(millis());
}

void TFTDisplay::displayMainMenu(menu_item_t menu)
{
    const uint32_t now = millis();
    if (scene == SC_MENU && menuCur != (int)menu && menuCur >= 0)
    {
        int items[24], pos;
        const int n = uiMenuSiblings(items, 24, &pos);
        int oldPos = pos;
        for (int i = 0; i < n; i++)
            if (items[i] == menuCur)
                oldPos = i;
        animDir = (pos == (oldPos + 1) % n) ? 1 : -1;
        animStart = now;
    }
    else
    {
        animStart = 0;
    }
    menuCur = menu;
    setScene(SC_MENU);
    render(now);
}

void TFTDisplay::setValueBounds(int min, int max, int saved)
{
    valMin = min;
    valMax = max;
    valSaved = saved;
    valIdx = saved;
}

void TFTDisplay::displayValue(menu_item_t menu, uint8_t value_index)
{
    const uint32_t now = millis();
    if (scene == SC_VALUE && valMenu == (int)menu && valIdx != value_index)
    {
        animDir = value_index > valIdx ? 1 : -1;
        animStart = now;
    }
    else
        animStart = 0;
    if (valMax < 0)
    {
        valMin = 0;
        valMax = getValueCount(menu) - 1;
        valSaved = value_index;
    }
    valMenu = menu;
    valIdx = value_index;
    setScene(SC_VALUE);
    render(now);
}

void TFTDisplay::displayBLEConfirm()
{
    confirmKind = CF_BLE;
    setScene(SC_CONFIRM);
    render(millis());
}

void TFTDisplay::displayBLEStatus()
{
    setScene(SC_BLE);
    render(millis());
}

void TFTDisplay::displayWiFiConfirm()
{
    confirmKind = CF_WIFI;
    setScene(SC_CONFIRM);
    render(millis());
}

void TFTDisplay::displayWiFiStatus()
{
    setScene(SC_WIFI);
    render(millis());
}

void TFTDisplay::displayBindConfirm()
{
    confirmKind = CF_BIND;
    setScene(SC_CONFIRM);
    render(millis());
}

void TFTDisplay::displayBindStatus()
{
    setScene(SC_BIND);
    render(millis());
}

void TFTDisplay::displayRunning()
{
    busyText = "RUNNING";
    setScene(SC_BUSY);
    render(millis());
}

void TFTDisplay::displaySending()
{
    busyText = "SENDING";
    setScene(SC_BUSY);
    render(millis());
}

void TFTDisplay::displayLinkstats()
{
    displayLinkstatsPage(0);
}

void TFTDisplay::displayLinkstatsPage(uint8_t page)
{
    statsPage = page;
    setScene(SC_STATS);
    render(millis());
}

#endif
