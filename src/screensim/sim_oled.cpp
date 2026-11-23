#include "sim_common.h"
#include "OLED/oleddisplay.h"

int main(int argc, char **argv)
{
    simLogoFile = "../hardware/logo/namimno.bin";
    OLEDDisplay d;
    d.init();
    return simScript(&d);
}
