#if defined(PLATFORM_ESP32) && !defined(PLATFORM_ESP32_C3)

#include <U8g2lib.h> // Needed for the OLED drivers, this is a arduino package. It is maintained by platformIO

#include <math.h>

#include "oleddisplay.h"

#include "OTA.h"
#include "XBMStrings.h" // Contains all the ELRS logos and animations for the UI
#include "common.h"
#include "logging.h"
#include "options.h"
#include "uimodel.h"
#include "qrcodegen/qrcodegen.h"

#include "WiFi.h"
extern WiFiMode_t wifiMode;

U8G2 *u8g2;

/*
 * Scene based renderer: the menu FSM calls the display*() entry points to
 * pick what is on screen, and tick() redraws it whenever something moves.
 * Every frame is drawn from scratch into the 1 KiB u8g2 buffer.
 */

typedef enum {
    SC_NONE,
    SC_SPLASH,
    SC_IDLE,
    SC_MENU,
    SC_VALUE,
    SC_CONFIRM,
    SC_BIND,
    SC_WIFI,
    SC_BLE,
    SC_BUSY,
    SC_STATS,
} scene_t;

typedef enum { CF_BIND, CF_WIFI, CF_BLE } confirm_t;

static scene_t scene = SC_NONE;
static uint32_t sceneStart;
static uint32_t lastFrame;

// Animated transitions
#define ANIM_MS 180
static uint32_t animStart;
static int animDir;

// Menu
static int menuCur = -1, menuPrev = -1;
// Value picker
static int valMenu, valIdx, valPrev, valMin, valMax = -1, valSaved;
// Confirm/busy
static confirm_t confirmKind;
static const char *busyText;
// Stats
static uint8_t statsPage, statsPrevPage;

// Logo cache for the splash
static uint8_t logo[128 * 64 / 8];
static bool logoValid;

#define F_TINY   u8g2_font_4x6_tr
#define F_SMALL  u8g2_font_5x7_tr
#define F_SMALLF u8g2_font_5x7_tf
#define F_MED    u8g2_font_6x10_tr
#define F_BOLD   u8g2_font_helvB08_tr
#define F_BOLD10 u8g2_font_helvB10_tr
#define F_BOLD12 u8g2_font_helvB12_tr
#define F_BIG    u8g2_font_logisoso16_tn
#define F_HUGE   u8g2_font_logisoso22_tn

// ---------------------------------------------------------------- primitives

static int textWidth(const char *s)
{
    return u8g2->getStrWidth(s);
}

static void drawCentered(int cx, int y, const char *s)
{
    u8g2->drawStr(cx - textWidth(s) / 2, y, s);
}

static void drawRight(int rx, int y, const char *s)
{
    u8g2->drawStr(rx - textWidth(s), y, s);
}

static void dottedH(int x0, int x1, int y, int step = 2)
{
    for (int x = x0; x <= x1; x += step)
        u8g2->drawPixel(x, y);
}

static void dottedV(int x, int y0, int y1, int step = 2)
{
    for (int y = y0; y <= y1; y += step)
        u8g2->drawPixel(x, y);
}

// Ordered-dither fill: level 0..16
static void ditherBox(int x0, int y0, int w, int h, int level)
{
    for (int y = y0; y < y0 + h; y++)
        for (int x = x0; x < x0 + w; x++)
            if (uiBayer4(x, y) < level)
                u8g2->drawPixel(x, y);
}

// Inverted rounded label, returns its width
static int pill(int x, int y, const char *s, bool inverted = true)
{
    u8g2->setFont(F_SMALL);
    const int w = textWidth(s) + 6;
    if (inverted)
    {
        u8g2->drawRBox(x, y, w, 9, 2);
        u8g2->setDrawColor(0);
        u8g2->drawStr(x + 3, y + 8, s);
        u8g2->setDrawColor(1);
    }
    else
    {
        u8g2->drawRFrame(x, y, w, 9, 2);
        u8g2->drawStr(x + 3, y + 8, s);
    }
    return w;
}

static void triangleUp(int cx, int y, int s)
{
    u8g2->drawTriangle(cx - s, y + s, cx + s, y + s, cx, y);
}

static void triangleDown(int cx, int y, int s)
{
    u8g2->drawTriangle(cx - s, y, cx + s, y, cx, y + s);
}

static void pageDots(int rx, int y, int n, int cur)
{
    for (int i = 0; i < n; i++)
    {
        const int x = rx - (n - 1 - i) * 6;
        if (i == cur)
            u8g2->drawDisc(x, y, 2);
        else
            u8g2->drawCircle(x, y, 2);
    }
}

// "!+" / "!-" in the pitmode strings become small up/down triangles
static int drawValueText(int x, int y, const char *s, bool measureOnly = false)
{
    char buf[24];
    int w = 0;
    while (*s)
    {
        int n = 0;
        while (*s && !(s[0] == '!' && (s[1] == '+' || s[1] == '-')) && n < 23)
            buf[n++] = *s++;
        buf[n] = 0;
        if (n)
        {
            if (!measureOnly)
                u8g2->drawStr(x + w, y, buf);
            w += textWidth(buf);
        }
        if (s[0] == '!')
        {
            const int h = u8g2->getAscent();
            if (!measureOnly)
            {
                if (s[1] == '+')
                    triangleUp(x + w + 4, y - h + 1, 3);
                else
                    triangleDown(x + w + 4, y - h + 2, 3);
            }
            w += 9;
            s += 2;
        }
    }
    return w;
}

static void drawValueCentered(int cx, int y, const char *s)
{
    const int w = drawValueText(0, 0, s, true);
    drawValueText(cx - w / 2, y, s);
}

// ---------------------------------------------------------------- icons

typedef struct {
    const unsigned char *img;
    uint8_t w, h;
} xbm_t;

static xbm_t iconFor(int menu)
{
    switch (menu)
    {
    case STATE_PACKET: return {rate_img64, 64, 44};
    case STATE_SWITCH: return {switch_img64, 64, 64};
    case STATE_ANTENNA: return {antenna_img64, 64, 64};
    case STATE_POWER:
    case STATE_POWER_MAX:
    case STATE_POWER_DYNAMIC: return {power_img64, 50, 50};
    case STATE_TELEMETRY: return {ratio_img64, 64, 64};
    case STATE_POWERSAVE: return {powersaving_img64, 64, 64};
    case STATE_SMARTFAN: return {fan_img64, 64, 64};
    case STATE_JOYSTICK: return {joystick_img64, 64, 59};
    case STATE_VTX:
    case STATE_VTX_BAND:
    case STATE_VTX_CHANNEL:
    case STATE_VTX_POWER:
    case STATE_VTX_PITMODE:
    case STATE_VTX_SEND: return {vtx_img64, 64, 64};
    case STATE_WIFI:
    case STATE_WIFI_TX: return {wifi_img64, 48, 44};
    case STATE_BIND: return {bind_img64, 64, 64};
    case STATE_WIFI_RX: return {rxwifi_img64, 64, 64};
    case STATE_WIFI_BACKPACK: return {backpack_img64, 64, 64};
    case STATE_WIFI_VRX: return {vrxwifi_img64, 64, 64};
    default: return {nullptr, 0, 0};
    }
}

// Draw an icon centred in a 64x64 cell at (x, y)
static void drawIcon(int menu, int x, int y)
{
    xbm_t i = iconFor(menu);
    if (i.img)
        u8g2->drawXBM(x + (64 - i.w) / 2, y + (64 - i.h) / 2, i.w, i.h, i.img);
}

// ---------------------------------------------------------------- common bits

// "1000mW" -> "1W" so it fits next to the staircase
static const char *shortPower(uint8_t idx)
{
    const char *p = Display::getValue(STATE_POWER, idx);
    if (!strcmp(p, "1000mW")) return "1W";
    if (!strcmp(p, "2000mW")) return "2W";
    return p;
}

// Use the biggest bold font that fits the width
static void fitBold(const char *s, int w)
{
    u8g2->setFont(F_BOLD12);
    if (textWidth(s) <= w) return;
    u8g2->setFont(F_BOLD10);
    if (textWidth(s) <= w) return;
    u8g2->setFont(F_BOLD);
}

static void fmtTimer(char *buf, size_t n, uint32_t ms)
{
    const uint32_t s = ms / 1000;
    snprintf(buf, n, "%02u:%02u", (unsigned)(s / 60) % 100, (unsigned)(s % 60));
}

static bool linkUp()
{
    return ui.link == UI_LINKED || ui.link == UI_ARMED || ui.link == UI_MISMATCH;
}

// Status bar: state pill on the left, temperature / flight timer on the right
static void statusBar(uint32_t now)
{
    const bool blink = (now / 400) & 1;
    char buf[16];
    switch (ui.link)
    {
    case UI_SEARCHING:
    {
        static const char *dots[] = {"SCAN", "SCAN.", "SCAN..", "SCAN..."};
        pill(0, 0, dots[(now / 300) % 4], false);
        break;
    }
    case UI_LINKED: pill(0, 0, "LINKED"); break;
    case UI_ARMED: pill(0, 0, "ARMED", blink || now - ui.armed_at > 3000); break;
    case UI_MISMATCH: pill(0, 0, "MISMATCH", blink); break;
    case UI_NOHANDSET: pill(0, 0, "NO HANDSET", blink); break;
    case UI_RADIOFAIL: pill(0, 0, "RADIO FAIL", blink); break;
    }

    u8g2->setFont(F_SMALLF);
    if (ui.link == UI_ARMED && ui.armed_at)
    {
        fmtTimer(buf, sizeof(buf), now - ui.armed_at);
        drawRight(127, 8, buf);
        // little "recording" dot
        if (blink)
            u8g2->drawDisc(127 - textWidth(buf) - 5, 4, 2);
    }
    else if (ui.temperature)
    {
        snprintf(buf, sizeof(buf), "%d\xb0" "C", ui.temperature);
        drawRight(127, 8, buf);
    }
    dottedH(0, 127, 10);
}

// ---------------------------------------------------------------- splash

static void drawSplash(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    // Logo dissolves in through the Bayer matrix over 900ms
    int level = t * 17 / 900;
    if (level > 16)
        level = 16;
    if (logoValid)
    {
        for (int y = 0; y < 64; y++)
            for (int x = 0; x < 128; x++)
                if ((logo[y * 16 + x / 8] >> (x & 7)) & 1 && uiBayer4(x, y) < level)
                    u8g2->drawPixel(x, y);
    }

    if (t > 900)
    {
        // version types itself out, then a scan line runs underneath
        char buf[32];
        snprintf(buf, sizeof(buf), "ELRS %s", uiVersion());
        const int len = strlen(buf);
        int shown = (t - 900) / 45;
        if (shown > len)
            shown = len;
        buf[shown] = 0;
        u8g2->setFont(F_TINY);
        u8g2->setDrawColor(0);
        u8g2->drawBox(0, 56, 128, 8);
        u8g2->setDrawColor(1);
        u8g2->drawStr(64 - (len * 4) / 2, 63, buf);
        if (shown < len && (now / 120) & 1)
            u8g2->drawBox(64 - (len * 4) / 2 + shown * 4, 58, 3, 5);
    }
    const int bar = t * 128 / 3000;
    u8g2->drawHLine(0, 55, bar > 128 ? 128 : bar);
}

// ---------------------------------------------------------------- idle

static void drawRate(int x, int y)
{
    char pre[8], num[8], suf[8];
    uiSplitRate(Display::getValue(STATE_PACKET, ui.rate), pre, num, suf);
    if (pre[0])
    {
        x += pill(x, y - 13, pre) + 2;
    }
    u8g2->setFont(F_BIG);
    u8g2->drawStr(x, y + 3, num);
    x += textWidth(num) + 2;
    u8g2->setFont(F_TINY);
    u8g2->drawStr(x, y + 3, suf);
}

// Ascending signal-strength style power staircase
static void drawPowerStairs(int x, int y, uint32_t now)
{
    const int n = ui.power_max - ui.power_min + 1;
    const int barW = n > 6 ? 3 : 4;
    for (int i = 0; i < n; i++)
    {
        const int lvl = ui.power_min + i;
        const int h = 3 + i * 11 / (n > 1 ? n - 1 : 1);
        const int bx = x + i * (barW + 1);
        const int by = y - h;
        if (lvl <= ui.run_power)
            u8g2->drawBox(bx, by, barW, h);
        else if (lvl <= ui.power)
            u8g2->drawFrame(bx, by, barW, h);
        else
            u8g2->drawPixel(bx + barW / 2, y - 1);
    }
    u8g2->setFont(F_MED);
    const int tx = x + n * (barW + 1) + 2;
    u8g2->drawStr(tx, y, shortPower(ui.run_power));
}

static void drawRadar(int cx, int cy, int r, uint32_t now)
{
    u8g2->drawCircle(cx, cy, r);
    for (int i = 4; i < r; i += 6)
        u8g2->drawPixel(cx, cy - i), u8g2->drawPixel(cx, cy + i), u8g2->drawPixel(cx - i, cy), u8g2->drawPixel(cx + i, cy);
    u8g2->drawCircle(cx, cy, r / 2, U8G2_DRAW_ALL);
    const float a = (now % 2000) / 2000.0f * 2 * M_PI;
    // the beam and a fading dotted trail behind it
    for (int k = 0; k < 6; k++)
    {
        const float ak = a - k * 0.12f;
        const int ex = cx + (int)(cosf(ak) * (r - 1));
        const int ey = cy + (int)(sinf(ak) * (r - 1));
        if (k == 0)
            u8g2->drawLine(cx, cy, ex, ey);
        else
            for (int s = 2; s < r; s += k + 1)
                u8g2->drawPixel(cx + (int)(cosf(ak) * s), cy + (int)(sinf(ak) * s));
    }
    // a couple of blips that flash when the beam passes them
    static const float blipA[] = {0.8f, 2.6f, 4.4f};
    static const float blipR[] = {0.55f, 0.8f, 0.35f};
    for (int b = 0; b < 3; b++)
    {
        float d = a - blipA[b];
        while (d < 0) d += 2 * M_PI;
        if (d < 1.2f)
        {
            const int bx = cx + (int)(cosf(blipA[b]) * r * blipR[b]);
            const int by = cy + (int)(sinf(blipA[b]) * r * blipR[b]);
            if (d < 0.4f)
                u8g2->drawDisc(bx, by, 1);
            else
                u8g2->drawPixel(bx, by);
        }
    }
}

// RSSI history as a dithered area plot (-110..-40 dBm), LQ dips marked underneath
static void drawRssiArea(int x0, int y0, int w, int h)
{
    const int n = uiHist.count < w ? uiHist.count : w;
    dottedH(x0, x0 + w - 1, y0, 4);
    int prevY = -1;
    for (int i = 0; i < n; i++)
    {
        const uint16_t idx = uiHistIdx(uiHist.count - n + i);
        const int8_t r = uiHist.rssi1[idx];
        const int x = x0 + w - n + i;
        if (r == 0)
        {
            prevY = -1;
            continue;
        }
        int v = (r + 110) * (h - 4) / 70;
        if (v < 0) v = 0;
        if (v > h - 4) v = h - 4;
        const int y = y0 + h - 4 - v;
        for (int yy = y + 1; yy < y0 + h - 3; yy++)
            if (uiBayer4(x, yy) < 4)
                u8g2->drawPixel(x, yy);
        if (prevY >= 0)
            u8g2->drawLine(x - 1, prevY, x, y);
        prevY = y;
        // LQ dips: a tick on the baseline, taller for worse
        const uint8_t lq = uiHist.lq[idx];
        if (lq < 98)
            u8g2->drawVLine(x, y0 + h - 1 - (100 - lq) / 25, 1 + (100 - lq) / 25);
    }
}

static void drawWarning(uint32_t now, const char *l1, const char *l2, const char *hint)
{
    // big hazard triangle
    const int cx = 22, top = 16;
    u8g2->drawTriangle(cx, top, cx - 19, top + 34, cx + 19, top + 34);
    u8g2->setDrawColor(0);
    u8g2->drawTriangle(cx, top + 6, cx - 13, top + 30, cx + 13, top + 30);
    u8g2->setDrawColor(1);
    if ((now / 500) & 1)
    {
        u8g2->drawBox(cx - 1, top + 12, 3, 10);
        u8g2->drawBox(cx - 1, top + 24, 3, 3);
    }
    u8g2->setFont(F_BOLD10);
    u8g2->drawStr(48, 30, l1);
    u8g2->drawStr(48, 44, l2);
    u8g2->setFont(F_TINY);
    u8g2->drawStr(48, 56, hint);
}

static void drawIdle(uint32_t now)
{
    statusBar(now);

    if (ui.link == UI_NOHANDSET)
    {
        drawWarning(now, "NO", "HANDSET", "check radio/bay");
        return;
    }
    if (ui.link == UI_RADIOFAIL)
    {
        drawWarning(now, "BAD", "RADIO", "power cycle TX");
        return;
    }

    // Left: rate, power staircase, telemetry ratio
    drawRate(0, 27);
    drawPowerStairs(0, 48, now);
    u8g2->setFont(F_TINY);
    u8g2->drawStr(0, 63, "TLM");
    u8g2->setFont(F_SMALL);
    u8g2->drawStr(14, 63, Display::getValue(STATE_TELEMETRY_CURR, ui.tlm));
    if (ui.dynamic)
    {
        // DYN tag, flashing while dynamic power is boosting above the floor
        const bool boost = ui.run_power != ui.power && ((now / 300) & 1);
        u8g2->setFont(F_TINY);
        if (!boost)
        {
            u8g2->drawRBox(43, 57, 17, 7, 1);
            u8g2->setDrawColor(0);
        }
        u8g2->drawStr(45, 63, "DYN");
        u8g2->setDrawColor(1);
    }

    dottedV(63, 13, 63);

    // Right: LQ and its history, or the radar while searching
    if (linkUp())
    {
        char buf[8];
        u8g2->setFont(F_TINY);
        u8g2->drawStr(67, 19, "LQ");
        u8g2->setFont(F_BIG);
        snprintf(buf, sizeof(buf), "%d", ui.lq_up);
        drawRight(121, 30, buf);
        u8g2->setFont(F_TINY);
        u8g2->drawStr(122, 30, "%");
        // RSSI under the "LQ" label
        snprintf(buf, sizeof(buf), "%ddB", ui.rssi_up1);
        u8g2->drawStr(67, 27, buf);
        drawRssiArea(66, 34, 62, 30);
    }
    else
    {
        drawRadar(96, 38, 24, now);
    }
}

// ---------------------------------------------------------------- menu

static void menuLabel(int menu, char *buf, size_t n)
{
    snprintf(buf, n, "%s %s", Display::menuString(menu, 0), Display::menuString(menu, 1));
}

static void drawMenu(uint32_t now)
{
    int items[24];
    int pos = 0;
    const int n = uiMenuSiblings(items, 24, &pos);
    float t = uiEase((now - animStart) / (float)ANIM_MS);
    const int slide = (int)((1.0f - t) * 22) * animDir; // list offset, px

    // Left: icon, sliding vertically
    u8g2->setClipWindow(0, 0, 63, 63);
    const int iconSlide = (int)((1.0f - t) * 64) * animDir;
    drawIcon(menuCur, 0, iconSlide);
    if (t < 1.0f && menuPrev >= 0)
        drawIcon(menuPrev, 0, iconSlide - 64 * animDir);
    u8g2->setMaxClipWindow();

    // Right: the wheel
    u8g2->setClipWindow(64, 0, 123, 63);
    char buf[24];
    for (int k = -2; k <= 2; k++)
    {
        if (n == 0) break;
        const int i = ((pos + k) % n + n) % n;
        const int m = items[i];
        const int cy = 32 + k * 22 + slide;
        if (k == 0)
            continue;
        u8g2->setFont(F_TINY);
        menuLabel(m, buf, sizeof(buf));
        u8g2->drawStr(68, cy + 3, buf);
    }
    // current item: inverted card
    u8g2->drawRBox(65, 20 + slide, 58, 25, 3);
    u8g2->setDrawColor(0);
    u8g2->setFont(F_BOLD);
    u8g2->drawStr(69, 31 + slide, Display::menuString(menuCur, 0));
    u8g2->setFont(F_SMALL);
    u8g2->drawStr(69, 41 + slide, Display::menuString(menuCur, 1));
    u8g2->setDrawColor(1);
    u8g2->setMaxClipWindow();

    // Scrollbar
    if (n > 1)
    {
        dottedV(126, 0, 63);
        const int th = 64 / n < 4 ? 4 : 64 / n;
        const int ty = pos * (64 - th) / (n - 1);
        u8g2->drawBox(125, ty, 3, th);
    }
}

// ---------------------------------------------------------------- value picker

static void drawValue(uint32_t now)
{
    char buf[24];
    // Header
    u8g2->drawBox(0, 0, 128, 11);
    u8g2->setDrawColor(0);
    u8g2->setFont(F_BOLD);
    menuLabel(valMenu, buf, sizeof(buf));
    u8g2->drawStr(2, 9, buf);
    u8g2->setFont(F_TINY);
    if (valMax >= valMin)
    {
        snprintf(buf, sizeof(buf), "%d/%d", valIdx - valMin + 1, valMax - valMin + 1);
        drawRight(126, 8, buf);
    }
    u8g2->setDrawColor(1);

    const float t = uiEase((now - animStart) / (float)ANIM_MS);
    const int slide = (int)((1.0f - t) * 15) * animDir;

    // Roller
    u8g2->setClipWindow(0, 12, 127, 56);
    for (int k = -2; k <= 2; k++)
    {
        const int idx = valIdx + k;
        if (k == 0 || idx < valMin || idx > valMax)
            continue;
        const char *v = Display::getValue(valMenu, idx);
        if (!v)
            continue;
        u8g2->setFont(F_SMALL);
        const int y = (k < 0 ? 21 + (k + 1) * 10 : 54 + (k - 1) * 10) + slide;
        drawValueCentered(64, y, v);
    }
    u8g2->setMaxClipWindow();

    // Selection window with the current value
    u8g2->setDrawColor(0);
    u8g2->drawBox(4, 24, 120, 21);
    u8g2->setDrawColor(1);
    u8g2->drawRFrame(4, 24, 120, 21, 4);
    u8g2->setFont(F_BOLD12);
    const char *cur = Display::getValue(valMenu, valIdx);
    if (cur)
        drawValueCentered(64, 40, cur);
    if (valIdx == valSaved)
    {
        // check mark: this is what is stored now
        u8g2->drawLine(9, 34, 11, 37);
        u8g2->drawLine(11, 37, 16, 30);
        u8g2->drawLine(10, 34, 12, 37);
    }
    if (valIdx > valMin)
        triangleUp(117, 28, 3);
    if (valIdx < valMax)
        triangleDown(117, 37, 3);

    // Footer hints
    u8g2->setFont(F_TINY);
    u8g2->drawStr(0, 63, "<BACK");
    drawRight(127, 63, valIdx == valSaved ? "SAVED" : "OK=SAVE");
}

// ---------------------------------------------------------------- confirm / status

static void drawConfirm(uint32_t now)
{
    const char *title = "", *sub = "";
    int icon = STATE_BIND;
    switch (confirmKind)
    {
    case CF_BIND: title = "BIND"; sub = "send request"; icon = STATE_BIND; break;
    case CF_WIFI: title = "WIFI"; sub = "update mode"; icon = STATE_WIFI; break;
    case CF_BLE: title = "GAMEPAD"; sub = "bluetooth"; icon = STATE_JOYSTICK; break;
    }
    // icon bobs gently
    const int bob = (int)(sinf((now - sceneStart) / 250.0f) * 2);
    drawIcon(icon, 0, bob);
    fitBold(title, 61);
    u8g2->drawStr(66, 22, title);
    u8g2->setFont(F_TINY);
    u8g2->drawStr(66, 32, sub);
    // pulsing button
    const bool on = ((now - sceneStart) / 450) & 1;
    u8g2->setFont(F_SMALL);
    if (on)
    {
        u8g2->drawRBox(66, 44, 58, 15, 4);
        u8g2->setDrawColor(0);
    }
    else
        u8g2->drawRFrame(66, 44, 58, 15, 4);
    drawCentered(95, 54, "PRESS OK");
    u8g2->setDrawColor(1);
}

static void drawRipples(int cx, int cy, uint32_t t, int maxR)
{
    for (int k = 0; k < 3; k++)
    {
        const int r = ((t / 25) + k * maxR / 3) % maxR;
        if (r > 2)
            u8g2->drawCircle(cx, cy, r);
    }
    u8g2->drawDisc(cx, cy, 3);
}

static void drawBind(uint32_t now)
{
    const uint32_t t = now - sceneStart;
    drawRipples(32, 32, t, 34);
    fitBold("BINDING", 61);
    u8g2->drawStr(66, 28, "BINDING");
    static const char *dots[] = {"", ".", "..", "..."};
    u8g2->setFont(F_SMALL);
    u8g2->drawStr(66, 40, "listening");
    u8g2->drawStr(66 + 45, 40, dots[(t / 300) % 4]);
    u8g2->setFont(F_TINY);
    u8g2->drawStr(66, 56, "put RX in bind");
}

static void drawSpinner(int cx, int cy, int r, uint32_t t)
{
    for (int k = 0; k < 8; k++)
    {
        const float a = k * M_PI / 4;
        const int phase = (t / 90 + k) % 8;
        const int x = cx + (int)(cosf(a) * r), y = cy + (int)(sinf(a) * r);
        if (phase > 4)
            u8g2->drawDisc(x, y, phase - 4);
        else
            u8g2->drawPixel(x, y);
    }
}

static void drawBusy(uint32_t now)
{
    drawSpinner(32, 32, 14, now - sceneStart);
    fitBold(busyText, 66);
    u8g2->drawStr(58, 37, busyText);
}

static void drawBLE(uint32_t now)
{
    const float t = (now - sceneStart) / 1000.0f;
    drawIcon(STATE_JOYSTICK, 0, 0);
    // a little stick gimbal doing lissajous figures
    const int gx = 104, gy = 44;
    u8g2->drawFrame(gx - 12, gy - 12, 25, 25);
    dottedH(gx - 11, gx + 11, gy, 3);
    dottedV(gx, gy - 11, gy + 11, 3);
    u8g2->drawDisc(gx + (int)(sinf(t * 2.1f) * 9), gy + (int)(sinf(t * 3.3f) * 9), 2);
    u8g2->setFont(F_BOLD);
    u8g2->drawStr(66, 12, "GAMEPAD");
    u8g2->setFont(F_TINY);
    u8g2->drawStr(66, 21, ((now / 500) & 1) ? "BLE LIVE" : "BLE");
}

static void drawQR(const char *text, int x, int y, int box)
{
    static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(4)];
    static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(4)];
    if (!qrcodegen_encodeText(text, tmp, qr, qrcodegen_Ecc_LOW, 1, 4, qrcodegen_Mask_AUTO, true))
        return;
    const int size = qrcodegen_getSize(qr);
    const int scale = (size + 2) * 2 <= box ? 2 : 1;
    const int ox = x + (box - size * scale) / 2;
    const int oy = y + (box - size * scale) / 2;
    // quiet zone: QR codes are dark-on-light, so draw on a lit square
    u8g2->drawBox(ox - 2, oy - 2, size * scale + 4, size * scale + 4);
    u8g2->setDrawColor(0);
    for (int my = 0; my < size; my++)
        for (int mx = 0; mx < size; mx++)
            if (qrcodegen_getModule(qr, mx, my))
                u8g2->drawBox(ox + mx * scale, oy + my * scale, scale, scale);
    u8g2->setDrawColor(1);
}

static void kv(int y, const char *k, const char *v)
{
    u8g2->setFont(F_TINY);
    u8g2->drawStr(66, y - 8, k);
    u8g2->setFont(F_SMALL);
    if (textWidth(v) > 61)
        u8g2->setFont(F_TINY);
    u8g2->drawStr(66, y, v);
}

static void drawWiFi(uint32_t now)
{
    char buf[80];
    if (wifiMode == WIFI_STA)
    {
        snprintf(buf, sizeof(buf), "http://%s.local", wifi_hostname);
        drawQR(buf, 0, 0, 64);
        snprintf(buf, sizeof(buf), "%s.local", wifi_hostname);
        kv(18, "OPEN IN BROWSER", buf);
        kv(38, "OR SCAN", "the code");
    }
    else
    {
        snprintf(buf, sizeof(buf), "WIFI:T:WPA;S:%s;P:%s;;", wifi_ap_ssid, wifi_ap_password);
        drawQR(buf, 0, 0, 64);
        kv(16, "SSID", wifi_ap_ssid);
        kv(36, "PASSWORD", wifi_ap_password);
        kv(56, "THEN OPEN", wifi_ap_address);
    }
}

// ---------------------------------------------------------------- link stats

static void statsHeader(const char *title, uint32_t now)
{
    pill(0, 0, title);
    pageDots(125, 4, 4, statsPage);
}

static void hbar(int x, int y, int w, int h, int pct)
{
    u8g2->drawFrame(x, y, w, h);
    const int f = (w - 2) * pct / 100;
    if (f > 0)
        u8g2->drawBox(x + 1, y + 1, f, h - 2);
}

static void drawStatsLink(uint32_t now)
{
    statsHeader("LINK", now);
    char buf[16];
    const bool up = linkUp();
    for (int side = 0; side < 2; side++)
    {
        const int x = side ? 67 : 0;
        const int lq = side ? ui.lq_dn : ui.lq_up;
        const int8_t r1 = side ? ui.rssi_dn1 : ui.rssi_up1;
        const int8_t r2 = side ? ui.rssi_dn2 : ui.rssi_up2;
        const int8_t snr = side ? ui.snr_dn : ui.snr_up;
        u8g2->setFont(F_TINY);
        u8g2->drawStr(x, 19, side ? "DOWNLINK" : "UPLINK");
        u8g2->setFont(F_BIG);
        snprintf(buf, sizeof(buf), "%d", up ? lq : 0);
        u8g2->drawStr(x, 37, buf);
        u8g2->setFont(F_TINY);
        u8g2->drawStr(x + textWidth(buf) * 0 + 34, 37, "LQ");
        hbar(x, 40, 60, 5, up ? lq : 0);
        u8g2->setFont(F_SMALL);
        if (r2 != 0)
            snprintf(buf, sizeof(buf), "%d/%d", r1, r2);
        else
            snprintf(buf, sizeof(buf), "%ddBm", r1);
        u8g2->drawStr(x, 54, up ? buf : "---");
        u8g2->setFont(F_TINY);
        snprintf(buf, sizeof(buf), "SNR %d", snr);
        u8g2->drawStr(x, 63, buf);
        if (!side)
        {
            snprintf(buf, sizeof(buf), "ANT%d", ui.ant + 1);
            drawRight(60, 63, buf);
        }
    }
    dottedV(63, 13, 63);
}

static int rssiY(int8_t r, int y0, int h)
{
    // -40..-120 dBm onto the plot
    int v = (-40 - r) * (h - 1) / 80;
    if (v < 0) v = 0;
    if (v > h - 1) v = h - 1;
    return y0 + v;
}

static void drawStatsScope(uint32_t now)
{
    statsHeader("RSSI", now);
    char buf[16];
    if (linkUp())
    {
        u8g2->setFont(F_SMALL);
        snprintf(buf, sizeof(buf), "%d", ui.rssi_up1);
        drawRight(98, 8, buf);
    }
    const int x0 = 17, y0 = 13, w = 111, h = 51;
    u8g2->setFont(F_TINY);
    const int8_t grid[] = {-40, -60, -80, -100, -120};
    for (int g = 0; g < 5; g++)
    {
        const int y = rssiY(grid[g], y0, h);
        dottedH(x0, x0 + w - 1, y, 4);
        if (g % 2 == 0)
        {
            snprintf(buf, sizeof(buf), "%d", grid[g]);
            drawRight(x0 - 2, y + 3 > 63 ? 63 : (y < 16 ? y + 5 : y + 3), buf);
        }
    }
    const int n = uiHist.count < w ? uiHist.count : w;
    int p1 = -1, p2 = -1;
    for (int i = 0; i < n; i++)
    {
        const uint16_t idx = uiHistIdx(uiHist.count - n + i);
        const int x = x0 + w - n + i;
        const int8_t r1 = uiHist.rssi1[idx], r2 = uiHist.rssi2[idx], rd = uiHist.rssi_dn[idx];
        if (r1)
        {
            const int y = rssiY(r1, y0, h);
            if (p1 >= 0) u8g2->drawLine(x - 1, p1, x, y);
            p1 = y;
        }
        else p1 = -1;
        if (r2)
        {
            const int y = rssiY(r2, y0, h);
            if ((x & 1) == 0) u8g2->drawPixel(x, y);
            p2 = y;
        }
        else p2 = -1;
        if (rd && (x % 4) == 0)
        {
            const int y = rssiY(rd, y0, h);
            u8g2->drawFrame(x - 1, y - 1, 3, 3);
        }
    }
    (void)p2;
}

static void drawStatsHops(uint32_t now)
{
    const int n = ui.hop_channels ? ui.hop_channels : 80;
    char buf[24];
    // overall telemetry hit rate across all hops
    uint32_t okSum = 0, cnt = 0;
    for (int ch = 0; ch < n; ch++)
        if (hopStats[ch].hits || hopStats[ch].ok)
            okSum += hopStats[ch].ok, cnt++;
    statsHeader("HOPS", now);
    u8g2->setFont(F_SMALL);
    if (cnt)
    {
        snprintf(buf, sizeof(buf), "%d%%", (int)(okSum * 100 / 255 / cnt));
        drawRight(98, 8, buf);
    }

    // per-channel telemetry success bars
    const int y0 = 13, h = 30, x0 = 2, w = 124;
    for (int ch = 0; ch < n; ch++)
    {
        const int x = x0 + ch * w / n;
        const int x2 = x0 + (ch + 1) * w / n - 1;
        const hopstat_t &s = hopStats[ch];
        if (!s.hits && !s.ok)
        {
            u8g2->drawPixel(x, y0 + h - 1);
            continue;
        }
        const int bh = 1 + s.ok * (h - 1) / 255;
        for (int xx = x; xx <= x2 && xx < x0 + w; xx++)
            u8g2->drawVLine(xx, y0 + h - bh, bh);
    }
    // RSSI heat strip, dithered
    const int sy = y0 + h + 3, sh = 9;
    for (int ch = 0; ch < n; ch++)
    {
        const int x = x0 + ch * w / n;
        const int x2 = x0 + (ch + 1) * w / n;
        const int lvl = uiRssiLevel(hopStats[ch].rssi) / 16;
        for (int xx = x; xx < x2; xx++)
            for (int yy = sy; yy < sy + sh; yy++)
                if (uiBayer4(xx, yy) < lvl)
                    u8g2->drawPixel(xx, yy);
    }
    u8g2->drawFrame(x0 - 1, sy - 1, w + 2, sh + 2);
    // current hop marker
    const int cx = x0 + hopStatsCurrent * w / n;
    triangleUp(cx, 56, 2);
    u8g2->setFont(F_TINY);
    u8g2->drawStr(0, 63, "CH0");
    snprintf(buf, sizeof(buf), "CH%d", n - 1);
    drawRight(127, 63, buf);
    drawCentered(64, 63, "OK% / RSSI");
}

static void drawStatsWaterfall(uint32_t now)
{
    statsHeader("WATERFALL", now);
    const int y0 = 12, rows = 52 < UI_WF_ROWS ? 52 : UI_WF_ROWS;
    // stretch the visible range across the 17 dither levels
    int lo = 255, hi = 0;
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < UI_WF_COLS; c++)
        {
            const uint8_t v = uiWaterfall[r][c];
            if (!v) continue;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
    if (hi <= lo) hi = lo + 1;
    for (int r = 0; r < rows; r++)
    {
        // newest row at the bottom, against the hop ticker; history rises
        const uint8_t *row = uiWaterfall[(uiWfHead + UI_WF_ROWS - r) % UI_WF_ROWS];
        const int y = y0 + rows - 1 - r;
        for (int x = 0; x < 128; x++)
        {
            const uint8_t v = row[x * UI_WF_COLS / 128];
            const int lvl = v ? 1 + (v - lo) * 15 / (hi - lo) : 0;
            if (uiBayer4(x, y) < lvl)
                u8g2->drawPixel(x, y);
        }
    }
    // current hop ticker along the bottom
    const int n = ui.hop_channels ? ui.hop_channels : 80;
    const int cx = hopStatsCurrent * 128 / n;
    u8g2->drawHLine(cx - 2, 63, 5);
    u8g2->drawPixel(cx, 62);
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
    u8g2->clearBuffer();
    u8g2->setDrawColor(1);
    u8g2->setFontMode(1);
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
    u8g2->sendBuffer();
    lastFrame = now;
}

static void setScene(scene_t s)
{
    if (scene != s)
        sceneStart = millis();
    scene = s;
}

// Frame interval per scene; 0 = only redraw on events
static uint32_t frameInterval(uint32_t now)
{
    const bool armed = ui.link == UI_ARMED;
    if (now - animStart < ANIM_MS + 40)
        return 25;
    switch (scene)
    {
    case SC_SPLASH: return 30;
    case SC_IDLE: return armed ? 500 : 66;
    case SC_STATS: return armed ? 500 : 100;
    case SC_CONFIRM: return 100;
    case SC_BIND:
    case SC_BLE:
    case SC_BUSY: return 40;
    case SC_WIFI: return 120;
    default: return 0;
    }
}

void OLEDDisplay::tick(uint32_t now)
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

static const u8g2_cb_t *pickRotation()
{
    if (OPT_SCREEN_MIRROR)
        return OPT_SCREEN_REVERSED ? U8G2_MIRROR_VERTICAL : U8G2_MIRROR;
    return OPT_SCREEN_REVERSED ? U8G2_R2 : U8G2_R0;
}

void OLEDDisplay::init()
{
    const u8g2_cb_t *rotation = pickRotation();
    if (OPT_HAS_OLED_SPI)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI(rotation, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_MOSI, GPIO_PIN_SCREEN_CS, GPIO_PIN_SCREEN_DC, GPIO_PIN_SCREEN_RST);
    else if (OPT_HAS_OLED_I2C)
        u8g2 = new U8G2_SSD1306_128X64_NONAME_F_HW_I2C(rotation, GPIO_PIN_SCREEN_RST, GPIO_PIN_SCREEN_SCK, GPIO_PIN_SCREEN_SDA);

    u8g2->begin();
    u8g2->clearBuffer();
}

void OLEDDisplay::doScreenBackLight(screen_backlight_t state)
{
    if (GPIO_PIN_SCREEN_BL != UNDEF_PIN)
    {
        digitalWrite(GPIO_PIN_SCREEN_BL, state);
    }
    if (state == SCREEN_BACKLIGHT_OFF)
    {
        u8g2->clearDisplay();
        u8g2->setPowerSave(true);
    }
    else
    {
        u8g2->setPowerSave(false);
    }
}

void OLEDDisplay::printScreenshot()
{
    u8g2->writeBufferXBM(*BackpackOrLogStrm);
}

void OLEDDisplay::dumpFrame(Print &out, int first, int count)
{
    dumpRows(out, "SCR 128 64 1", u8g2->getBufferPtr(), 64, 16, first, count);
}

void OLEDDisplay::displaySplashScreen()
{
    logoValid = spi_flash_read(logo_image, logo, sizeof(logo)) == ESP_OK;
    setScene(SC_SPLASH);
    sceneStart = millis();
    render(millis());
}

void OLEDDisplay::displayIdleScreen(uint8_t changed, uint8_t rate_index, uint8_t power_index, uint8_t ratio_index, uint8_t motion_index, uint8_t fan_index, bool dynamic, uint8_t running_power_index, uint8_t temperature, message_index_t message_index)
{
    storeIdle(rate_index, power_index, ratio_index, dynamic, running_power_index, temperature);
    const bool fresh = scene != SC_IDLE;
    setScene(SC_IDLE);
    if (fresh || changed)
        render(millis());
}

void OLEDDisplay::displayMainMenu(menu_item_t menu)
{
    const uint32_t now = millis();
    if (scene == SC_MENU && menuCur != (int)menu && menuCur >= 0)
    {
        // Work out the direction of travel from the sibling order
        int items[24], pos;
        const int n = uiMenuSiblings(items, 24, &pos);
        int oldPos = pos;
        for (int i = 0; i < n; i++)
            if (items[i] == menuCur)
                oldPos = i;
        animDir = (pos == (oldPos + 1) % n) ? 1 : -1;
        menuPrev = menuCur;
        animStart = now;
    }
    else
    {
        menuPrev = -1;
        animStart = 0;
    }
    menuCur = menu;
    setScene(SC_MENU);
    render(now);
}

void OLEDDisplay::setValueBounds(int min, int max, int saved)
{
    valMin = min;
    valMax = max;
    valSaved = saved;
    valIdx = valPrev = saved;
}

void OLEDDisplay::displayValue(menu_item_t menu, uint8_t value_index)
{
    const uint32_t now = millis();
    if (scene == SC_VALUE && valMenu == (int)menu && valIdx != value_index)
    {
        animDir = value_index > valIdx ? 1 : -1;
        animStart = now;
    }
    else
    {
        animStart = 0;
    }
    if (valMax < 0)
    {
        // setValueBounds was not called: fall back to the whole list
        valMin = 0;
        valMax = getValueCount(menu) - 1;
        valSaved = value_index;
    }
    valPrev = valIdx;
    valMenu = menu;
    valIdx = value_index;
    setScene(SC_VALUE);
    render(now);
}

void OLEDDisplay::displayBLEConfirm()
{
    confirmKind = CF_BLE;
    setScene(SC_CONFIRM);
    render(millis());
}

void OLEDDisplay::displayBLEStatus()
{
    setScene(SC_BLE);
    render(millis());
}

void OLEDDisplay::displayWiFiConfirm()
{
    confirmKind = CF_WIFI;
    setScene(SC_CONFIRM);
    render(millis());
}

void OLEDDisplay::displayWiFiStatus()
{
    setScene(SC_WIFI);
    render(millis());
}

void OLEDDisplay::displayBindConfirm()
{
    confirmKind = CF_BIND;
    setScene(SC_CONFIRM);
    render(millis());
}

void OLEDDisplay::displayBindStatus()
{
    setScene(SC_BIND);
    render(millis());
}

void OLEDDisplay::displayRunning()
{
    busyText = "RUNNING";
    setScene(SC_BUSY);
    render(millis());
}

void OLEDDisplay::displaySending()
{
    busyText = "SENDING";
    setScene(SC_BUSY);
    render(millis());
}

void OLEDDisplay::displayLinkstats()
{
    displayLinkstatsPage(0);
}

void OLEDDisplay::displayLinkstatsPage(uint8_t page)
{
    statsPrevPage = statsPage;
    statsPage = page;
    setScene(SC_STATS);
    render(millis());
}

#endif
