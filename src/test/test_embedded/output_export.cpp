#include <Arduino.h>
#include <output_export.h>



#ifdef __GNUC__
void output_start(unsigned int baudrate __attribute__((unused)))
#else
void output_start(unsigned int baudrate)
#endif
{
    Serial.begin(115200);
}

void output_char(int c)
{
    Serial.write(c);
}

void output_flush(void)
{
    Serial.flush();
}

void output_complete(void)
{
   Serial.end();
}