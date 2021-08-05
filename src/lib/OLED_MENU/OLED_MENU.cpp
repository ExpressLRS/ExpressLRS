/* 
 * This file is part of the ExpressLRS distribution (https://github.com/ExpressLRS/ExpressLRS).
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAS_I2C_OLED_MENU

#include "targets.h"
#include "OLED_MENU.h"

#include "XBMStrings.h" // Contains all the express logos and animation for UI


U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(U8G2_R2, GPIO_PIN_OLED_SCL, GPIO_PIN_OLED_SDA);

extern void setRGBColor(uint8_t color);

void shortPressCallback(void);
void longPressCallback(void);

#define LOCKTIME 5000

menuShow_t OLED_MENU::currentItem[] ={
    {80,0,PKTRATE,pktRateLPCB,getRateString},

    {80,1,TLMRADIO,tlmLPCB,getTLMRatioString},

    {80,2,POWERLEVEL,powerLPCB,getPowerString},

    {80,3,RGBCOLOR,rgbLPCB,getRgbString},

    {80,4,BINDING,bindLPCB,getBindString},

    {80,5,WIFIUPDATE,updateLPCB,getUpdateiString},
};

showString_t OLED_MENU::showString[] ={
    "PktRate ",
    "TLM ",
    "Power ",
    "Rgb ",
    "Bind ",
    "Update ",
};

uint32_t OLED_MENU::lastProcessTime=0;
uint8_t OLED_MENU::currentIndex = 0;
uint8_t OLED_MENU::showBaseIndex = 0;
uint8_t OLED_MENU::screenLocked = 0;

void shortPressCallback(void)
{
    OLED_MENU::shortPressCB();
}


void longPressCallback(void)
{
    OLED_MENU::longPressCB();
}

void OLED_MENU::displayLockScreen()
{
    u8g2.begin();
    u8g2.clearBuffer();

    u8g2.drawXBM(16, 16, 64, 64, elrs64); 

    u8g2.sendBuffer();
}

void OLED_MENU::updateScreen(void)
{
    uint32_t now = millis();
    u8g2.clearBuffer();

    if(now - lastProcessTime > LOCKTIME) // LOCKTIME seconds later lock the screen
    {
        screenLocked = 1;
        u8g2.drawXBM(16, 16, 64, 64, elrs64);  
    }
    else
    {
        u8g2.setFont(u8g2_font_courR10_tr);
        for(int i=showBaseIndex;i<(4+showBaseIndex);i++)
        {
            u8g2.drawStr(0, (i-showBaseIndex)*15+12, showString[i].str); //the row line num: 12 , 27 , 42 ,57
            if(currentIndex == currentItem[i].index)
            {
                u8g2.drawStr(currentItem[i].line-10,(i-showBaseIndex)*15+12, ">");
            }
            
            u8g2.drawStr(currentItem[i].line,(i-showBaseIndex)*15+12, currentItem[i].getStr(currentItem[i].value));
        }
    }
    u8g2.sendBuffer();
}

void OLED_MENU::shortPressCB(void)
{
    if(!screenLocked)
    {
        lastProcessTime = millis();
        currentIndex ++;

        if(currentIndex > (sizeof(currentItem)/sizeof(menuShow_t) -1)) //detect last index,then comeback to first
        {
            currentIndex = 0;
            showBaseIndex =0;
        }
        if(currentIndex > 3) //each screen only shows 4 line menu
        {
            showBaseIndex ++;
        }
    }
}

void OLED_MENU::longPressCB(void)
{
    lastProcessTime = millis();
    if(screenLocked)  //unlock screen
    {
        screenLocked = 0;       
    }
    else
    {  
        currentItem[currentIndex].lpcb(currentIndex);
    }  
}

void OLED_MENU::pktRateLPCB(uint8_t i)
{
    currentItem[i].value++;

    if(currentItem[i].value>7)
    {
        currentItem[i].value = 0;
    }

    /*
    TO DO: 需要添加对应的处理部分
    */

}

void OLED_MENU::tlmLPCB(uint8_t i)
{
    currentItem[i].value++;

    if(currentItem[i].value>7)
    {
        currentItem[i].value = 0;
    }

    /*
    TO DO: 需要添加对应的处理部分
    */

}

void OLED_MENU::powerLPCB(uint8_t i)
{
    currentItem[i].value++;

    if(currentItem[i].value>7)
    {
        currentItem[i].value = 0;
    }

    /*
    TO DO: 需要添加对应的处理部分
    */

}

void OLED_MENU::rgbLPCB(uint8_t i)
{
    currentItem[i].value++;

    if(currentItem[i].value>8)
    {
        currentItem[i].value = 0;
    }

    /*
    TO DO: 需要添加对应的处理部分
    */
   setRGBColor(currentItem[i].value);
}

void OLED_MENU::bindLPCB(uint8_t i)
{   
    /*
    TO DO: 需要添加对应的处理部分
    */

}

void OLED_MENU::updateLPCB(uint8_t i)
{
    /*
    TO DO: 需要添加对应的处理部分
    */

}


const char * OLED_MENU::getBindString(int bind)
{
    return "bind";
}


const char * OLED_MENU::getUpdateiString(int update)
{
    return "update";
}


const char * OLED_MENU::getRgbString(int rgb)
{
    switch (rgb)
    {
    case 0: return "off";
    case 1: return "cyan";
    case 3: return "blue";
    case 4: return "green";
    case 5: return "white";
    case 6: return "violet";
    case 7: return "yellow";
    case 2: return "magenta";
    default: return "Error";
    }
}

/**
 * Returns power level string (Char array)
 *
 * @param values power = int/enum for current power level.  
 * @return const char array for power level Ex: "500 mW\0"
 */
const char *OLED_MENU::getPowerString(int power){
    switch (power)
    {
    case 0: return "10mW";
    case 1: return "25mW";
    case 3: return "100mW";
    case 4: return "250mW";
    case 5: return "500mW";
    case 6: return "1000mW";
    case 7: return "2000mW";
    case 2: return "50mW";
    default: return "Error";
    }
}

/**
 * Returns packet rate string (Char array)
 *
 * @param values rate = int/enum for current packet rate.  
 * @return const char array for packet rate Ex: "500 hz\0"
 */
const char *OLED_MENU::getRateString(int rate){
    switch (rate)
    {
    case 0: return "500hz";
    case 1: return "250hz";
    case 2: return "200hz";
    case 3: return "150hz";
    case 4: return "100hz";
    case 5: return "50hz";
    case 6: return "25hz";
    case 7: return "4hz";
    default: return "ERROR";
    }
}

/**
 * Returns telemetry ratio string (Char array)
 *
 * @param values ratio = int/enum for current power level.  
 * @return const char array for telemetry ratio Ex: "1:128\0"
 */
const char *OLED_MENU::getTLMRatioString(int ratio){
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