#include "sim_common.h"
#include "WiFi.h"
#include "options.h"
#include <SPI.h>
#include <Wire.h>

uint32_t sim_ms = 0;
uint32_t millis() { return sim_ms; }
uint32_t micros() { return sim_ms * 1000; }

SPIClass SPI;
TwoWire Wire;

class NullOut : public Stream
{
public:
    size_t write(uint8_t) override { return 1; }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
} nullOut;
Stream *BackpackOrLogStrm = &nullOut;

bool OtaIsFullRes = false;
WiFiMode_t wifiMode = WIFI_AP;
bool simWifiSta = false;
const char *wifi_hostname = "elrs_tx";
const char *wifi_ap_ssid = "ExpressLRS TX";
const char *wifi_ap_password = "expresslrs";
const char *wifi_ap_address = "10.0.0.1";
uint32_t logo_image = 0;
const char *simLogoFile = nullptr;

int spi_flash_read(uint32_t, void *dst, size_t n)
{
    memset(dst, 0, n);
    if (!simLogoFile) return 1;
    FILE *f = fopen(simLogoFile, "rb");
    if (!f) return 1;
    fread(dst, 1, n, f);
    fclose(f);
    return ESP_OK;
}

std::vector<int> simMenu;
int simMenuPos = 0;

void uiPullReal(ui_model_t &m)
{
    m.dual = false;
    m.hop_channels = 80;
    m.power_min = 1;
    m.power_max = 6;
    if (!m.demo)
        m.link = UI_NOHANDSET;
}

int uiMenuSiblings(int *items, int max, int *pos)
{
    int n = 0;
    for (int v : simMenu)
        if (n < max) items[n++] = v;
    *pos = simMenuPos;
    return n;
}

const char *uiVersion() { return "4.1.0"; }

void simRun(Display *d, uint32_t ms)
{
    for (uint32_t t = 0; t < ms; t += 10)
    {
        sim_ms += 10;
        d->tick(sim_ms);
    }
}

void simShot(Display *d, const char *name)
{
    std::string p = std::string(getenv("SIM_OUT") ? getenv("SIM_OUT") : "out") + "/" + name + ".scr";
    FilePrint fp(p.c_str());
    d->dumpFrame(fp);
}

static void idleParams(Display *d, uint8_t rate, uint8_t power, uint8_t run, uint8_t tlm, bool dyn)
{
    d->displayIdleScreen(0xFF, rate, power, tlm, 0, 0, dyn, run, 41, MSG_NONE);
}

static void menuGo(Display *d, int pos)
{
    simMenuPos = pos;
    d->displayMainMenu((menu_item_t)simMenu[pos]);
}

int simScript(Display *d)
{
    uiDemoSet(true);
    // Splash
    sim_ms = 1;
    d->displaySplashScreen();
    simRun(d, 400);  simShot(d, "01_splash_a");
    simRun(d, 500);  simShot(d, "02_splash_b");
    simRun(d, 1600); simShot(d, "03_splash_c");

    // Idle: demo timeline is searching 0-7s, linked 7-26s, armed 26-58s (80s cycle)
    sim_ms = 2000;
    idleParams(d, 4, 4, 2, 3, true); // 500Hz, 250mW max, running 50mW, 1:8
    simRun(d, 1500); simShot(d, "10_idle_search");
    simRun(d, 16500); simShot(d, "11_idle_linked");
    idleParams(d, 0, 6, 6, 5, false); // F1000Hz, 1W, 1:32
    simRun(d, 500); simShot(d, "12_idle_linked_f1000");
    idleParams(d, 5, 4, 4, 2, false); // 333 Full
    simRun(d, 15000); simShot(d, "13_idle_armed");
    ui.demo_force = UI_NOHANDSET;
    simRun(d, 600); simShot(d, "14_idle_nohandset");
    ui.demo_force = UI_MISMATCH;
    simRun(d, 300); simShot(d, "15_idle_mismatch");
    ui.demo_force = -1;
    idleParams(d, 4, 4, 2, 3, true);

    // Link stats pages (still armed in the demo timeline, so history is busy)
    for (int p = 0; p < 4; p++)
    {
        d->displayLinkstatsPage(p);
        simRun(d, 1000);
        char n[32];
        snprintf(n, sizeof n, "2%d_stats_%d", p, p);
        simShot(d, n);
    }

    // Menu
    simMenu = {STATE_PACKET, STATE_TELEMETRY, STATE_SWITCH, STATE_POWER, STATE_JOYSTICK, STATE_BIND, STATE_WIFI, STATE_VTX};
    menuGo(d, 0); simRun(d, 300); simShot(d, "30_menu_packet");
    menuGo(d, 1); simRun(d, 70); simShot(d, "31_menu_anim");
    simRun(d, 300); simShot(d, "32_menu_tlm");
    menuGo(d, 3); simRun(d, 300); simShot(d, "33_menu_power");
    menuGo(d, 6); simRun(d, 300); simShot(d, "34_menu_wifi");

    // Value pickers
    d->setValueBounds(0, 9, 4);
    d->displayValue(STATE_PACKET, 4); simRun(d, 300); simShot(d, "40_value_rate_saved");
    d->displayValue(STATE_PACKET, 5); simRun(d, 60); simShot(d, "41_value_anim");
    simRun(d, 300); simShot(d, "42_value_rate");
    d->setValueBounds(0, 21, 2);
    d->displayValue(STATE_VTX_PITMODE, 3); simRun(d, 300); simShot(d, "43_value_pitmode");
    d->setValueBounds(1, 6, 3);
    d->displayValue(STATE_POWER_MAX, 6); simRun(d, 300); simShot(d, "44_value_power_max");

    // Confirm / status screens
    d->displayBindConfirm(); simRun(d, 300); simShot(d, "50_bind_confirm");
    d->displayBindStatus(); simRun(d, 700); simShot(d, "51_binding");
    d->displayWiFiConfirm(); simRun(d, 700); simShot(d, "52_wifi_confirm");
    wifiMode = WIFI_AP;
    d->displayWiFiStatus(); simRun(d, 300); simShot(d, "53_wifi_ap");
    wifiMode = WIFI_STA;
    d->displayWiFiStatus(); simRun(d, 300); simShot(d, "54_wifi_sta");
    d->displayBLEConfirm(); simRun(d, 300); simShot(d, "55_ble_confirm");
    d->displayBLEStatus(); simRun(d, 700); simShot(d, "56_ble");
    d->displaySending(); simRun(d, 300); simShot(d, "57_sending");
    return 0;
}
