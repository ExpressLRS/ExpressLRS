#include "uimodel.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

ui_model_t ui = {};
uint32_t uiFreezeUntil = 0;
ui_history_t uiHist = {};
uint8_t uiWaterfall[UI_WF_ROWS][UI_WF_COLS];
uint8_t uiWfHead = 0;
uint32_t uiWfRows = 0;

static uint32_t lastHistMs = 0;
static uint32_t lastWfMs = 0;
static ui_link_e prevLink = UI_SEARCHING;

#define HIST_PERIOD_MS 250
#define WF_PERIOD_MS   500

// ---------------------------------------------------------------- helpers

static const uint8_t bayer4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

uint8_t uiBayer4(int x, int y)
{
    return bayer4[y & 3][x & 3];
}

float uiEase(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

uint8_t uiRssiLevel(int8_t rssi)
{
    if (rssi == 0) return 0;
    int v = (rssi + 120) * 255 / 90;
    if (v < 1) v = 1;
    if (v > 255) v = 255;
    return v;
}

const char *uiLinkName(ui_link_e l)
{
    switch (l)
    {
    case UI_SEARCHING: return "SEARCHING";
    case UI_LINKED:    return "LINKED";
    case UI_ARMED:     return "ARMED";
    case UI_MISMATCH:  return "MODEL MISMATCH";
    case UI_NOHANDSET: return "NO HANDSET";
    case UI_RADIOFAIL: return "RADIO FAIL";
    }
    return "";
}

// "DK500Hz" -> "DK" "500" "Hz", "333 Full" -> "" "333" "FULL"
void uiSplitRate(const char *s, char *pre, char *num, char *suf)
{
    int p = 0, n = 0, f = 0;
    while (*s && (*s < '0' || *s > '9'))
        pre[p++] = *s++;
    while (*s >= '0' && *s <= '9')
        num[n++] = *s++;
    while (*s == ' ')
        s++;
    const bool upper = (*s != 'H'); // Hz stays mixed case, Full becomes FULL
    while (*s)
    {
        char c = *s++;
        suf[f++] = (upper && c >= 'a' && c <= 'z') ? c - 32 : c;
    }
    pre[p] = num[n] = suf[f] = 0;
}

// ---------------------------------------------------------------- demo

static uint32_t rng = 0x1234567;
static uint32_t xrand()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float frand() { return (xrand() & 0xFFFF) / 65535.0f; }

void uiDemoSet(bool on)
{
    ui.demo = on;
    ui.demo_force = -1;
    hopStatsReset();
}

void uiDemoCycle()
{
    ui.demo = true;
    ui.demo_force++;
    if (ui.demo_force > UI_RADIOFAIL)
        ui.demo_force = -1;
}

static void demoLink(uint32_t now)
{
    const float t = now / 1000.0f;
    const uint32_t cyc = (now / 1000) % 80;

    ui_link_e l;
    if (ui.demo_force >= 0)
        l = (ui_link_e)ui.demo_force;
    else if (cyc < 7)
        l = UI_SEARCHING;
    else if (cyc >= 26 && cyc < 58)
        l = UI_ARMED;
    else
        l = UI_LINKED;
    ui.link = l;

    if (l != UI_LINKED && l != UI_ARMED && l != UI_MISMATCH)
    {
        ui.lq_up = ui.lq_dn = 0;
        ui.rssi_up1 = ui.rssi_up2 = ui.rssi_dn1 = ui.rssi_dn2 = 0;
        ui.snr_up = ui.snr_dn = 0;
        return;
    }

    // Slow fade plus a fast flutter, worse while "flying" (armed)
    const float range = (l == UI_ARMED) ? 22.0f : 8.0f;
    float base = -62.0f - range * (0.5f + 0.5f * sinf(t * 0.21f)) - 6.0f * sinf(t * 1.3f);
    float r1 = base + (frand() - 0.5f) * 6.0f;
    float r2 = base - 4.0f + 7.0f * sinf(t * 0.7f) + (frand() - 0.5f) * 6.0f;
    float rd = base - 3.0f + (frand() - 0.5f) * 8.0f;
    ui.rssi_up1 = (int8_t)r1;
    ui.rssi_up2 = (int8_t)r2;
    ui.rssi_dn1 = (int8_t)rd;
    ui.rssi_dn2 = 0;
    ui.snr_up = (int8_t)((r1 + 105.0f) / 3.0f);
    ui.snr_dn = (int8_t)((rd + 105.0f) / 3.0f);
    ui.ant = (r2 > r1) ? 1 : 0;

    float best = r1 > r2 ? r1 : r2;
    int lq = 100 - (int)((-best - 78.0f) * 3.0f) - (int)(frand() * 3.0f);
    if (lq > 100) lq = 100;
    if (lq < 8) lq = 8;
    ui.lq_up = lq;
    int lqd = lq - 2 - (int)(frand() * 4.0f);
    ui.lq_dn = lqd < 0 ? 0 : lqd;

    // Telemetry slots: a "WiFi" burst parks on channels 18..40 every so often,
    // plus fading that walks across the band.
    const int ch_count = ui.hop_channels ? ui.hop_channels : 80;
    const bool wifiBurst = fmodf(t, 9.0f) < 4.5f;
    for (int i = 0; i < 24; i++)
    {
        int ch = xrand() % ch_count;
        float p = lq / 100.0f;
        float fade = 0.5f + 0.5f * sinf(ch * 0.35f + t * 0.8f);
        float chRssi = best - 10.0f * fade + (frand() - 0.5f) * 4.0f;
        if (wifiBurst && ch >= 18 && ch <= 40)
        {
            p *= 0.35f + 0.5f * fabsf(sinf(ch * 0.9f + t));
        }
        if (ch >= 60 && ch <= 63)
            p *= 0.6f; // a permanently noisy spot
        if (frand() < p)
            hopStatsHit(ch, (int8_t)chRssi);
        else
            hopStatsMiss(ch);
    }
}

// ---------------------------------------------------------------- update

static void sample(uint32_t now)
{
    if (ui.link == UI_SEARCHING || ui.link == UI_NOHANDSET || ui.link == UI_RADIOFAIL)
    {
        // keep sliding the graph so it visibly flatlines
        uiHist.lq[uiHist.head] = 0;
        uiHist.rssi1[uiHist.head] = 0;
        uiHist.rssi2[uiHist.head] = 0;
        uiHist.rssi_dn[uiHist.head] = 0;
    }
    else
    {
        uiHist.lq[uiHist.head] = ui.lq_up;
        uiHist.rssi1[uiHist.head] = ui.rssi_up1;
        uiHist.rssi2[uiHist.head] = ui.rssi_up2;
        uiHist.rssi_dn[uiHist.head] = ui.rssi_dn1;
    }
    uiHist.head = (uiHist.head + 1) % UI_HIST_LEN;
    if (uiHist.count < UI_HIST_LEN)
        uiHist.count++;
}

static void waterfallRow()
{
    const uint8_t *prev = uiWaterfall[uiWfHead];
    uiWfHead = (uiWfHead + 1) % UI_WF_ROWS;
    uint8_t *row = uiWaterfall[uiWfHead];
    const int n = ui.hop_channels ? ui.hop_channels : 80;
    for (int c = 0; c < UI_WF_COLS; c++)
    {
        // bin channels onto columns
        int ch0 = c * n / UI_WF_COLS;
        int ch1 = (c + 1) * n / UI_WF_COLS;
        if (ch1 <= ch0) ch1 = ch0 + 1;
        // What happened on these hops since the last row: best hit RSSI,
        // knocked down by any misses. Unvisited cells fade out from the row above.
        int best = 0, miss = 0;
        bool visited = false;
        for (int ch = ch0; ch < ch1 && ch < HOPSTATS_MAX_CHANNELS; ch++)
        {
            hopstat_t &s = hopStats[ch];
            if (s.rowBest || s.rowMiss)
                visited = true;
            const int lvl = uiRssiLevel(s.rowBest);
            if (lvl > best) best = lvl;
            miss += s.rowMiss;
            s.rowBest = 0;
            s.rowMiss = 0;
        }
        if (visited)
        {
            int v = best >> (miss > 8 ? 8 : miss);
            row[c] = v ? v : 1; // 1 = visited but nothing heard
        }
        else
        {
            row[c] = prev[c] * 7 / 8;
        }
    }
    uiWfRows++;
}

void uiModelUpdate(uint32_t now)
{
    uiPullReal(ui); // returns early for the link fields when ui.demo is set
    if (ui.demo)
        demoLink(now);

    const bool linked = ui.link == UI_LINKED || ui.link == UI_ARMED || ui.link == UI_MISMATCH;
    if (linked && ui.linked_at == 0)
        ui.linked_at = now | 1;
    if (!linked)
        ui.linked_at = 0;
    if (ui.link == UI_ARMED && prevLink != UI_ARMED)
        ui.armed_at = now | 1;
    if (ui.link != UI_ARMED)
        ui.armed_at = 0;
    prevLink = ui.link;

    if (now - lastHistMs >= HIST_PERIOD_MS)
    {
        lastHistMs = now;
        sample(now);
    }
    if (now - lastWfMs >= WF_PERIOD_MS)
    {
        lastWfMs = now;
        waterfallRow();
    }
}
