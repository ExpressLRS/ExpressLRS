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

#pragma once

class OLED
{

private:


public:
    static void displayLogo();
    const char * getPowerString(int power);
    const char * getRateString(int rate);
    const char * getTLMRatioString(int ratio);
    void setCommitString(const uint8_t * commit, char * commitStr);
    static void updateScreen(const char * power, const char * rate, const char * ratio, const char * commitStr);
};