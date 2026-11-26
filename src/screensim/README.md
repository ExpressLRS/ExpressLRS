# screensim

Builds the real `lib/SCREEN` OLED and TFT renderers for the PC, so screens can be iterated on without flashing.

```sh
sh build.sh                       # needs native gcc/g++ and a prior pio build of an ESP32 TX env (for U8g2 / Arduino_GFX)
./sim_oled.exe                    # writes out/*.scr
SIM_OUT=out_tft ./sim_tft.exe
python render.py out              # *.png at 4x, plus sheet.png
```

On hardware, build with `-DDEBUG_SCREEN_REMOTE` to drive the screen over the USB port instead of a handset:

* `u d l r o` short press, `U D L R O` long press
* `!` dumps the framebuffer, `@NNN` re-sends one row
* `#` cycles the demo link state, `~` toggles the demo generator
* `?` prints the menu state, `j` streams the joystick ADC

`remote.py PORT U wait:0.5 d shot:menu` chains keys and screenshots. `flash.sh ENV TARGET PORT [configurator opts]` configures a build and writes only the app slot.

USB CRSF input is disabled in these builds.
