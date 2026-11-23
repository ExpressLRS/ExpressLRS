// Shared harness for the screen simulators
#pragma once
#include <Arduino.h>
#include <vector>
#include <string>
#include "uimodel.h"
#include "display.h"

extern uint32_t sim_ms;
extern std::vector<int> simMenu;   // the sibling list uiMenuSiblings reports
extern int simMenuPos;
extern const char *simLogoFile;
extern bool simWifiSta;

class FilePrint : public Print
{
public:
    FILE *f;
    explicit FilePrint(const char *path) { f = fopen(path, "wb"); }
    ~FilePrint() { if (f) fclose(f); }
    size_t write(uint8_t c) override { fputc(c, f); return 1; }
};

void simRun(Display *d, uint32_t ms);          // advance time, ticking the display
void simShot(Display *d, const char *name);    // write out/<name>.scr
int simScript(Display *d);                     // the shared shot list
