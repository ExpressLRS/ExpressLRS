

enum LEDstates {led_off, solid_red, solid_green, solid_blue, flash_red, flash_green, flash_blue, fade_red, fade_green, fade_blue};

LEDstates LEDstate = led_off;

extern byte LEDpin;

int fadeCounter = 0;
int BlinkInterval = 5;
int FadeTime = 500;
bool LEDblinkState = false;


unsigned long LEDtimerLastChecked = 0;
int LEDtimerTickRate = 100; //led timer tickrate in milliseconds

int ButTickRate = 100; //Button timer tickrate in milliseconds
int BuzTickRate = 100; //Buzzer timer tickrate in milliseconds

void SetLED(byte r, byte g, byte b) {
  //analogWrite(LEDpin, (r<<2));

}

void ProcessLed() {
  if  (millis() > LEDtimerLastChecked + LEDtimerTickRate) {
    HandleLed();
    LEDtimerLastChecked = millis();
  }
}

void HandleLed() {
  switch (LEDstate) {
    case led_off:
      return;
    case solid_red:
      SetLED(255, 0, 0);
    case solid_green:
      SetLED(0, 255, 0);
    case solid_blue:
      SetLED(0, 0, 255);

    case flash_red:

      fadeCounter = fadeCounter + 1;
      if (fadeCounter == BlinkInterval) {
        if (fadeCounter) {
          SetLED(255, 0, 0);
        } else {
          SetLED(0, 0, 0);
        }

        LEDblinkState = !LEDblinkState;
        fadeCounter = 0;
      }

    case flash_green:

      fadeCounter = fadeCounter + 1;
      if (fadeCounter == BlinkInterval) {
        if (fadeCounter) {
          SetLED(0, 255, 0);
        } else {
          SetLED(0, 0, 0);
        }

        LEDblinkState = !LEDblinkState;
        fadeCounter = 0;
      }

    case flash_blue:

      fadeCounter = fadeCounter + 1;
      if (fadeCounter == BlinkInterval) {
        if (fadeCounter) {
          SetLED(0, 0, 255);
        } else {
          SetLED(0, 0, 0);
        }

        LEDblinkState = !LEDblinkState;
        fadeCounter = 0;
      }


    default:
      break;
  }
}

void SetLEDmode(LEDstates LEDstate_);   //need to define this twice as arduino is fucked with respect to enums and this is a workaround
void SetLEDmode(LEDstates LEDstate_) {
  switch (LEDstate_) {
    case led_off:
      SetLED(0, 0, 0);
      LEDtimerTickRate = 100;
    default:
      break;
  }
}



