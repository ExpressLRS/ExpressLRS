#include "targets.h"
#include "common.h"
#include "device.h"

#include "helpers.h"
#include "logging.h"

// Even though we aren't using anything this keeps the PIO dependency analyzer happy!
#include "POWERMGNT.h"

#if defined(GPIO_PIN_BUZZER) && (GPIO_PIN_BUZZER != UNDEF_PIN)

static void initializeBuzzer()
{
    pinMode(GPIO_PIN_BUZZER, OUTPUT);
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

static int playTune(bool start, const int tune[][2], int numTones)
{
    static uint8_t tunepos = 0;
    if (start)
    {
        pinMode(GPIO_PIN_BUZZER, OUTPUT);
        tunepos = 0;
    }
    if (tunepos > numTones)
    {
        noTone(GPIO_PIN_BUZZER);
        pinMode(GPIO_PIN_BUZZER, INPUT);
        tunepos = 0;
        return DURATION_NEVER;
    }
    tone(GPIO_PIN_BUZZER, tune[tunepos][0], tune[tunepos][1]);
    tunepos++;
    return tune[tunepos-1][1];
}

static int updateBuzzer(bool start)
{
    static bool startComplete = false;
    if (connectionState == radioFailed)
    {
        return playTune(start, failedTune, ARRAY_SIZE(failedTune));
    }
#if !defined(DISABLE_STARTUP_BEEP)
    else if (!startComplete)
    {
        int delay = playTune(start, melody, ARRAY_SIZE(melody));
        if (delay == DURATION_NEVER)
        {
            startComplete = true;
        }
        return delay;
    }
#endif
    else if (connectionState == noCrossfire)
    {
        return playTune(start, noCrossfireTune, ARRAY_SIZE(noCrossfireTune));
    }
#if !defined(DISABLE_STARTUP_BEEP)
    else if (connectionState < MODE_STATES)
    {
        return playTune(start, crossfireTune, ARRAY_SIZE(crossfireTune));
    }
#endif
    return DURATION_NEVER;
}

static int event(std::function<void ()> sendSpam)
{
    return updateBuzzer(true);
}

static int timeout(std::function<void ()> sendSpam)
{
    return updateBuzzer(false);
}

device_t Buzzer_device = {
    .initialize = initializeBuzzer,
    .start = NULL,
    .event = event,
    .timeout = timeout
};

#else

device_t Buzzer_device = {
    NULL
};

#endif
