#!/bin/sh
# Build the PC screen simulators. Needs a native gcc/g++ and a prior PIO build
# of an ESP32 TX env (for the U8g2 / Arduino_GFX library sources).
set -e
cd "$(dirname "$0")"
LIBDEPS=../.pio/libdeps/Unified_ESP32S3_2400_TX_via_UART
[ -d "$LIBDEPS" ] || LIBDEPS=../.pio/libdeps/Unified_ESP32_2400_TX_via_UART
U8G2="$LIBDEPS/U8g2/src"
GFX="$LIBDEPS/GFX Library for Arduino/src"
mkdir -p obj out
DEFS="-DARDUINO=10819 -DSCREEN_SIM -DPLATFORM_ESP32 -DRADIO_SX128X"
INC="-Ishim -I../lib/SCREEN -I$U8G2"
CXX="g++ -std=gnu++17 -O1 -g -w $DEFS $INC"

if [ ! -f obj/libu8g2.a ]; then
  for f in "$U8G2"/clib/*.c; do gcc -O1 -w $DEFS -c "$f" -o obj/u8g2_$(basename "$f" .c).o & done; wait
  $CXX -c "$U8G2/U8g2lib.cpp" -o obj/U8g2lib.o
  $CXX -c "$U8G2/U8x8lib.cpp" -o obj/U8x8lib.o
  ar rcs obj/libu8g2.a obj/u8g2_*.o obj/U8g2lib.o obj/U8x8lib.o
fi

SCREEN="../lib/SCREEN/display.cpp ../lib/SCREEN/uimodel.cpp ../lib/SCREEN/hopstats.cpp sim_common.cpp"
gcc -O1 -w -c ../lib/SCREEN/qrcodegen/qrcodegen.c -o obj/qrcodegen.o

case "${1:-all}" in
  oled|all)
    $CXX sim_oled.cpp ../lib/SCREEN/OLED/oleddisplay.cpp $SCREEN obj/qrcodegen.o obj/libu8g2.a -o sim_oled.exe
    ;;
esac
case "${1:-all}" in
  tft|all)
    if [ -f sim_tft.cpp ]; then
      if [ ! -f obj/libgfx.a ]; then
        for f in "$GFX/Arduino_GFX.cpp" "$GFX/Arduino_G.cpp" "$GFX/canvas/Arduino_Canvas.cpp"; do
          $CXX "-I$GFX" -c "$f" -o "obj/gfx_$(basename "$f" .cpp).o"
        done
        ar rcs obj/libgfx.a obj/gfx_*.o
      fi
      $CXX "-I$GFX" sim_tft.cpp ../lib/SCREEN/TFT/tftdisplay.cpp $SCREEN obj/qrcodegen.o obj/libgfx.a obj/libu8g2.a -o sim_tft.exe
    fi
    ;;
esac
echo built
