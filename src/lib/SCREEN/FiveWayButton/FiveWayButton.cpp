#ifdef HAS_FIVE_WAY_BUTTON
#include "FiveWayButton.h"

#define KEY_DEBOUNCE_TIME   500
#define KEY_LONG_RPESS_TIME 1000
#define KEY_CHECK_OVER_TIME 2000


uint32_t key_debounce_start = 0;

int key = INPUT_KEY_NO_PRESS;


#ifdef GPIO_PIN_JOYSTICK
#define UP    0;
#define DOWN  1;
#define LEFT  2;
#define RIGHT 3;
#define ENTER 4;
#define IDLE  5;

static int16_t joyAdcValues[] = JOY_ADC_VALUES;

int16_t fuzzValues[] = {0,0,0,0,0,0};
int16_t sortedJoyAdcValues[] = JOY_ADC_VALUES; //We are going to sort these at in init()


void swap(int16_t* xp, int16_t* yp)
{
    int16_t temp = *xp;
    *xp = *yp;
    *yp = temp;
}
 

//  Thanks alot Jumper for making this whole process more complicated than it has to be.
// Function to perform Selection Sort
void selectionSort()
{
    int i, j, min_idx;
 
    int n = 6;

    // One by one move boundary of unsorted subarray
    for (i = 0; i < n - 1; i++) {
 
        // Find the minimum element in unsorted array
        min_idx = i;
        for (j = i + 1; j < n; j++)
            if (sortedJoyAdcValues[j] < sortedJoyAdcValues[min_idx])
                min_idx = j;
 
        // Swap the found minimum element
        // with the first element
        swap(&sortedJoyAdcValues[min_idx], &sortedJoyAdcValues[i]);
    }
}

void findMinDiff()
{
    int n = 6; //Number of positions up, down, left, right, enter, idle
    
    // First for loop is stepping through the the real JOY_ADC_VALUES
    for (int i=0; i < n; i++)
        // Second for loop is stepping through the sorted values to find its min
        // distance between (curr - 1) and (curr + 1)
        for (int j = 0; j < n; j++)
            if(joyAdcValues[i] == sortedJoyAdcValues [j] ){
                // If the value is the lowest sorted value
                if(j == 0 ){
                    fuzzValues[i] = (sortedJoyAdcValues[1] - sortedJoyAdcValues[0])/2;
                // Else if the value is the largest sorted value
                } else if (j == (n - 1)){
                    fuzzValues[i] = (sortedJoyAdcValues[n - 1] - sortedJoyAdcValues[n -2])/2;
                // else the value is in between two other values
                } else {
                    int16_t beforeValueFuzz = (sortedJoyAdcValues[j] - sortedJoyAdcValues[j -1]);
                    int16_t afterValueFuzz = (sortedJoyAdcValues[j + 1] - sortedJoyAdcValues[j]);
                    if (beforeValueFuzz < afterValueFuzz){
                        fuzzValues[i] = beforeValueFuzz / 2;
                    } else {
                        fuzzValues[i] = afterValueFuzz / 2;
                    }
                    
                }
            }
    int test = 0;
}

bool checkValue(int direction){ 
    int value = analogRead(GPIO_PIN_JOYSTICK);
    if(value < (joyAdcValues[direction] + 50) && value > (joyAdcValues[direction] - 50)){
        return true;
    } else {
        return false;
    }
}

int checkKey()
{ 
    // int fuzz = (findMinDiff(joyAdcValues, 6) /2 );
    int value = analogRead(GPIO_PIN_JOYSTICK);
        
    if(value < (joyAdcValues[0] + fuzzValues[0]) && value > (joyAdcValues[0] - fuzzValues[0]))
        return INPUT_KEY_UP_PRESS;
    else if(value < (joyAdcValues[1] + fuzzValues[1]) && value > (joyAdcValues[1] - fuzzValues[1]))
        return INPUT_KEY_DOWN_PRESS;
    else if(value < (joyAdcValues[2] + fuzzValues[2]) && value > (joyAdcValues[2] - fuzzValues[2]))
        return INPUT_KEY_LEFT_PRESS;
    else if(value < (joyAdcValues[3] + fuzzValues[3]) && value > (joyAdcValues[3] - fuzzValues[3]))
        return INPUT_KEY_RIGHT_PRESS;
    else if(value < (joyAdcValues[4] + fuzzValues[4]) && value > (joyAdcValues[4] - fuzzValues[4]))
        return INPUT_KEY_OK_PRESS;
    else
        return INPUT_KEY_NO_PRESS;
}
#endif

void FiveWayButton::init()
{ 
    #ifndef JOY_ADC_VALUES
    pinMode(GPIO_PIN_FIVE_WAY_INPUT1, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT2, INPUT|PULLUP );
    pinMode(GPIO_PIN_FIVE_WAY_INPUT3, INPUT|PULLUP );
    #else 
    selectionSort();
    findMinDiff();
    #endif

    key_state = INPUT_KEY_NO_PRESS;
    keyPressed = false;
    isLongPressed = false;
}

void FiveWayButton::handle()
{  
    if(keyPressed)
    {
        #ifndef JOY_ADC_VALUES
        int key_down = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        #else 
        // Hack, we know that INPUT_KEY_NO_PRESS = 7 so for analog we would have to have it equal idle 
        int key_down = checkKey();
        #endif


        if(key_down == INPUT_KEY_NO_PRESS)
        {
            //key released
            if(isLongPressed)
            {
                clearKeyState();
            }
            else
            {
                key_state = key;
                if((millis() - key_debounce_start) > KEY_DEBOUNCE_TIME)
                {
                    keyPressed = false;        
                }
            }
        }
        else
        {
            if((millis() - key_debounce_start) > KEY_LONG_RPESS_TIME)
            {
                isLongPressed = true;
            }
        }
    }
    else
    {
        #ifndef JOY_ADC_VALUES
        key = digitalRead(GPIO_PIN_FIVE_WAY_INPUT1) << 2 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT2) << 1 |  digitalRead(GPIO_PIN_FIVE_WAY_INPUT3);
        #else 
        key = checkKey();
        #endif
        
        if(key != INPUT_KEY_NO_PRESS)
        {
            keyPressed = true;
            isLongPressed = false;
            key_debounce_start = millis();
        }
    }   
}

void FiveWayButton::getKeyState(int *keyValue, bool *keyLongPressed)
{
    *keyValue = key_state;
    *keyLongPressed = isLongPressed;
    if(key_state != INPUT_KEY_NO_PRESS)
    {
        clearKeyState();
    }
}

void FiveWayButton::clearKeyState()
{
   keyPressed = false;
   isLongPressed = false;
   key_state = INPUT_KEY_NO_PRESS;
}
#endif