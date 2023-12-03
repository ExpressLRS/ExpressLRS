#ifndef H_PPM
#define H_PPM

#include "targets.h"
#include "crsf_protocol.h"
#include "common.h"
#include "Arduino.h"
#include "esp32-hal.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "logging.h"


#define PPM_RESOLUTION_NS 1000 // 1uS TICK
#define PPM_MIN_SYMBOL_TICKS  10  //   10us
#define PPM_MAX_SYMBOL_TICKS  2200 //  3ms max

extern "C" void receive_process(uint32_t *data, size_t len, void * arg);

class PPMReader {
  private:
    rmt_obj_t* rmt_recv = NULL;
    float realNanoTick;
    uint32_t lastActiveMS=0;
  public:
    int8_t gpio = -1;

    PPMReader(uint8_t pin, float rmtFreqHz) 
    {
        if ((rmt_recv = rmtInit(pin, RMT_RX_MODE, RMT_MEM_192)) == NULL)
        {
            DBGLN("init receiver failed\n");        
            return;
        }
        gpio = pin;
        realNanoTick = rmtSetTick(rmt_recv, rmtFreqHz);
        rmtSetFilter(rmt_recv, true,PPM_MIN_SYMBOL_TICKS);
        rmtSetRxThreshold(rmt_recv,PPM_MAX_SYMBOL_TICKS);
        DBGLN("PPM INIT OK");
    }
    void begin() {
        // Creating RMT RX Callback Task
      DBGLN("BEGIN PPM RECV TASK");
      rmtRead(rmt_recv, receive_process, this);
    }


    void process(rmt_data_t *data, size_t len)
    {
        // DBGLN("RECV PPM SIG [%d] realTick[%f]",len,realNanoTick);
        if(len>7 ){//has good packet
          lastActiveMS=millis();
        }
        for (size_t i = 0; i<len-1; i++) {
        // DBGLN("PPM CHANNEL VALUE %d [%d | %d | %d | %d]",i,data[i].level0 ,data[i].duration0,data[i].level1,data[i].duration1);
        uint16_t value= (data[i].duration1 + data[i+1].duration0);
        if(value > 900 && value < 2250){
            ChannelData[i]=fmap(value,1000,2000,CRSF_CHANNEL_VALUE_MIN,CRSF_CHANNEL_VALUE_MAX);
        }
      }

    }
    bool isActive(){
      uint32_t check=millis() - lastActiveMS;
      return (check>500)? false:true;
    }

};


void receive_process(uint32_t *data, size_t len, void * arg)
{
  PPMReader * p = (PPMReader *)arg;
  p->process((rmt_data_t*) data, len);
}
#endif