#pragma once

#include <stdint.h>
#include "hopstats.h"

/*
 * Display-independent state for the OLED/TFT UI.
 *
 * Everything the screens draw comes from here, so the same model can be fed
 * from the live link (uiPullReal) or from the bench demo generator, and so
 * the drawing code can be compiled and rendered on a PC (screensim/).
 */

typedef enum : uint8_t {
    UI_SEARCHING,
    UI_LINKED,
    UI_ARMED,
    UI_MISMATCH,
    UI_NOHANDSET,
    UI_RADIOFAIL,
} ui_link_e;

#define UI_HIST_LEN 128 // 4 Hz samples -> 32 s of history
#define UI_WF_ROWS  48
#define UI_WF_COLS  80

typedef struct {
    // From the idle-screen parameters
    uint8_t rate;        // index into the rate strings
    uint8_t power;       // configured (max) power index
    uint8_t run_power;   // power currently in use
    uint8_t tlm;         // index into the current-ratio strings
    uint8_t dynamic;     // dynamic power enabled
    uint8_t temperature; // degC
    uint8_t power_min;   // lowest selectable power index
    uint8_t power_max;   // highest selectable power index

    // Link
    ui_link_e link;
    uint8_t lq_up, lq_dn;
    int8_t rssi_up1, rssi_up2, rssi_dn1, rssi_dn2;
    int8_t snr_up, snr_dn;
    uint8_t ant;
    bool dual;           // two receive antennas/radios on the TX
    uint8_t hop_channels;// FHSS channel count in use
    uint32_t armed_at;   // millis() when arming was first seen, 0 if not armed
    uint32_t linked_at;  // millis() when the link came up, 0 if not linked

    bool demo;           // demo generator owns the link fields
    int8_t demo_force;   // -1 = scripted timeline, else a forced ui_link_e
} ui_model_t;

typedef struct {
    uint8_t lq[UI_HIST_LEN];
    int8_t rssi1[UI_HIST_LEN];
    int8_t rssi2[UI_HIST_LEN];
    int8_t rssi_dn[UI_HIST_LEN];
    uint16_t head;  // next write position
    uint16_t count; // valid samples
} ui_history_t;

extern ui_model_t ui;
extern uint32_t uiFreezeUntil; // millis() until which screens stop re-rendering (remote screenshots)
extern ui_history_t uiHist;
extern uint8_t uiWaterfall[UI_WF_ROWS][UI_WF_COLS]; // 0 = nothing, 1..255 intensity
extern uint8_t uiWfHead;                             // row written most recently
extern uint32_t uiWfRows;                            // rows written in total

// Sample history index: i = 0 is the oldest valid sample
static inline uint16_t uiHistIdx(uint16_t i)
{
    return (uiHist.head + UI_HIST_LEN - uiHist.count + i) % UI_HIST_LEN;
}

void uiModelUpdate(uint32_t now);
void uiDemoSet(bool on);
void uiDemoCycle();

// Implemented by the platform (menu.cpp on the TX, the simulator on a PC)
void uiPullReal(ui_model_t &m);
int uiMenuSiblings(int *items, int max, int *pos);
const char *uiVersion();

// Helpers
void uiSplitRate(const char *s, char *pre, char *num, char *suf);
float uiEase(float t);
uint8_t uiBayer4(int x, int y);
const char *uiLinkName(ui_link_e l);
uint8_t uiRssiLevel(int8_t rssi); // -120..-30 dBm -> 0..255
