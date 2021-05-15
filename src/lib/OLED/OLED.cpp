#ifdef HAS_OLED

#include "targets.h"
#include "OLED.h"
#include "POWERMGNT.h"
#include <U8g2lib.h>
#include "XBMStrings.h"

#if defined HAS_OLED_128_32
// https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
// https://www.winstar.com.tw/products/oled-module/graphic-oled-display/wearable-displays.html
// Used on the Ghost TX Lite
U8G2_SSD1306_128X32_UNIVISION_F_4W_SW_SPI u8g2(U8G2_R0, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
#else
// https://github.com/olikraus/u8g2/wiki/u8g2setupcpp
U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R0, GPIO_PIN_OLED_SCK, GPIO_PIN_OLED_MOSI, GPIO_PIN_OLED_CS, GPIO_PIN_OLED_DC, GPIO_PIN_OLED_RST);
#endif

void helper(int x, int y, int size,  const unsigned char * image){
    u8g2.clearBuffer();
    u8g2.drawXBMP(x, y, size, size, image);
    u8g2.sendBuffer();
}

void easterEgg(){
    // Using i < 16 and (i*4) to get 64 total pixels. Change to i < 32 (i*2) to slow animation. 
    for(int i = 0; i < 16; i++){
        u8g2.clearBuffer();
        u8g2.drawXBMP(32, 16, 32, 32, ghost);
        u8g2.drawXBMP((-30 + (i*4)), 16, 32, 32, elrs32);
        u8g2.sendBuffer();
    }
    helper(28,12,40,elrs40);
    helper(24,8,48,elrs48);
    helper(20,4,56,elrs56);
    helper(16,0,64,elrs64);
}

void OLED::displayLogo(){
    u8g2.begin();
    u8g2.clearBuffer();
    #if defined HAS_OLED_128_32
    u8g2.drawXBM(48, 0, 32, 32, elrs32);
    #else
        #if defined TARGET_TX_GHOST
            easterEgg();
        #else
            u8g2.drawXBM(16, 16, 64, 64, elrs64);
        #endif
    #endif
    u8g2.sendBuffer();
}

void OLED::updateScreen(const char * power, const char * rate, const char * ratio, const char * commit){
    u8g2.clearBuffer();

    #if defined HAS_OLED_128_32
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(0,15, rate);
        u8g2.setFont(u8g2_font_courR10_tr);
        u8g2.drawStr(70,15 , ratio);
        u8g2.drawStr(0,32, power);
        u8g2.setFont(u8g2_font_courR08_tr);
        u8g2.drawStr(80,28, "TELEM");
    #else
        u8g2.setFont(u8g2_font_courR08_tr);
        u8g2.drawStr(0,10, "ExpressLRS");
        u8g2.drawStr(12,10, commit);
        u8g2.drawStr(0,20, rate);
        u8g2.drawStr(0,30, ratio);
        u8g2.drawStr(0,40, power);
        u8g2.drawStr(0,50, "Bind");
        u8g2.drawStr(0,60, "Wifi Update");
    #endif
    u8g2.sendBuffer();
}




const char * OLED::getPowerString(int power){
    switch (power)
    {
    case 0: return "10 mW";
    case 1: return "25 mW";
    case 3: return "100 mW";
    case 4: return "250 mW";
    case 5: return "500 mmW";
    case 6: return "1000 mW";
    case 7: return "2000 mW";
    case 2: return "50 mW";
    default: return "Error";
    }
}

const char * OLED::getRateString(int rate){
    switch (rate)
    {
    case 0: return "500 Hz";
    case 1: return "250 Hz";
    case 2: return "200 Hz";
    case 3: return "150 Hz";
    case 4: return "100 Hz";
    case 5: return "50 Hz";
    case 6: return "25 Hz";
    case 7: return "4 Hz";
    default: return "ERROR";
    }
}

const char * OLED::getTLMRatioString(int ratio){
    switch (ratio)
    {
    case 0: return "OFF";
    case 1: return "1:128";
    case 2: return "1:64";
    case 3: return "1:32";
    case 4: return "1:16";
    case 5: return "1:8";
    case 6: return "1:4";
    case 7: return "1:2";
    default: return "error";
    }
}

#endif