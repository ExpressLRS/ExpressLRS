#include "sim_common.h"
#include "TFT/tftdisplay.h"

int main(int argc, char **argv)
{
    simLogoFile = "../hardware/logo/runcam_tft.bin";
    TFTDisplay d;
    d.init();
    return simScript(&d);
}
