#if defined(TARGET_UBER_TX)

#include "targets.h"
#include <SPIFFS.h>

static void *hardware[HARDWARE_LAST];

void hardware_init()
{
    HardwareSerial serialPort(2);
    serialPort.begin(460800, SERIAL_8N1, 3, 1);

    for (int i=0 ; i<HARDWARE_LAST ; i++) hardware[i] = (void *)-1;

    SPIFFS.begin();
    File file = SPIFFS.open("/hardware.ini", "r");
    if (!file) {
        return;
    }
    do {
        String line = file.readStringUntil('\n');
        if (line.charAt(0)!=';' && line.indexOf('=') > 0) {
            int element = line.substring(0, line.indexOf('=')).toInt();
            String value = line.substring(line.indexOf('=') + 1);
            if (value.indexOf('.') != -1) {
                // it has a . so it'a a float
                hardware[element] = new float;
                *(float *)hardware[element] = value.toFloat();
            } else if (value.indexOf(',') != -1) {
                // it has a ',' so it's an array
                std::vector<int16_t> numbers;
                size_t pos = 0;
                while ((pos = value.indexOf(',')) != -1) {
                    numbers.push_back(atoi(value.substring(0, pos).c_str()));
                    value = value.substring(pos + 1);
                }
                numbers.push_back(atoi(value.c_str()));
                hardware[element] = new int16_t[numbers.size()];
                for (int i = 0 ; i<numbers.size() ; i++) {
                    ((int16_t *)hardware[element])[i] = numbers[i];
                }
            } else {
                hardware[element] = (void *)value.toInt();
            }
        }
    } while(file.available());
    file.close();

    serialPort.end();
    SPIFFS.end();
}

const int hardware_pin(nameType name)
{
    return (int)hardware[name];
}

const bool hardware_flag(nameType name)
{
    return (int)hardware[name] <= 0 ? false : true;
}

const int hardware_int(nameType name)
{
    return (int)hardware[name];
}

const float hardware_float(nameType name)
{
    return *(float*)hardware[name];
}

const int16_t* hardware_i16_array(nameType name)
{
    return (int16_t*)hardware[name];
}

const uint16_t* hardware_u16_array(nameType name)
{
    return (uint16_t*)hardware[name];
}

#endif