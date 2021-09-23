#if defined(TARGET_TX) 

#include "targets.h"
#include "common.h"
#include "device.h"
#include "helpers.h"

#if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)

static void initializeBuzzer()
{
}

static const int failedTune[][2] = {{480, 200},{400, 200}};
static const int crossfireTune[][2] = {{520, 150},{676, 300}};
static const int noCrossfireTune[][2] = {{676, 300},{520, 150}};
#if defined(MY_STARTUP_MELODY_ARR)
// It's silly but I couldn't help myself. See: BLHeli32 startup tones.
const int melody[][2] = MY_STARTUP_MELODY_ARR;
#elif defined(JUST_BEEP_ONCE)
static const int melody[][2] = {{400,200},{480,200}};
#else
static const int melody[][2] = {{659,300},{659,300},{523,100},{659,300},{783,550},{392,575}};
#endif

bool playTune(const int tune[][2], int numTones, unsigned long now)
{
    static unsigned long lastUpdate = 0;
    static uint8_t tunepos = 0;
    if ((tunepos == 0 || (int)(now - lastUpdate) > tune[tunepos-1][1]) && tunepos < numTones)
    {
        tone(GPIO_PIN_BUZZER, tune[tunepos][0], tune[tunepos][1]);
        tunepos++;
        lastUpdate = now;
        return false;
    }
    else if (tunepos > numTones)
    {
        noTone(GPIO_PIN_BUZZER);
        pinMode(GPIO_PIN_BUZZER, INPUT);
        tunepos = 0;
        return true;
    }
    return false;
}

static bool updateBuzzer(bool eventFired, unsigned long now)
{
    static bool startComplete = false;
    static connectionState_e lastState = disconnected;

    if (connectionState == radioFailed && lastState != radioFailed)
    {
        if(playTune(failedTune, ARRAY_SIZE(failedTune), now))
        {
            lastState = connectionState;
        }
    }
#if !defined(DISABLE_STARTUP_BEEP)
    else if (!startComplete)
    {
        startComplete = playTune(melody, ARRAY_SIZE(melody), now);
        lastState = connectionState;
    }
#endif
    else if (connectionState == noCrossfire && lastState != noCrossfire)
    {
        if (playTune(noCrossfireTune, ARRAY_SIZE(noCrossfireTune), now))
        {
            lastState = noCrossfire;
        }
    }
    else if (connectionState < MODE_STATES && lastState == noCrossfire)
    {
#if !defined(DISABLE_STARTUP_BEEP)
        if (playTune(crossfireTune, ARRAY_SIZE(crossfireTune), now))
#endif
        {
            lastState = connectionState;
        }
    }
    return false;
}

device_t Buzzer_device = {
    initializeBuzzer,
    updateBuzzer
};

#else

device_t Buzzer_device = {
    NULL,
    NULL
};

#endif

#endif