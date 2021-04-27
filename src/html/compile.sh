#!/bin/bash
#
# Make sure you install css-html-js-minify
# pip install css-html-js-minify
#
css-html-js-minify --quiet --zipy .

echo "static const char PROGMEM PNG[] = {" > ../src/flag_png.h
cat flag.png | xxd -i >> ../src/flag_png.h
echo "};" >> ../src/flag_png.h

echo "static const char PROGMEM CSS[] = {" > ../src/main_css.h
cat main.min.css.gz | xxd -i >> ../src/main_css.h
echo "};" >> ../src/main_css.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP32_WebContent.h
cat tx_index.html | gzip -9 | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h
echo "" >> ../src/ESP32_WebContent.h
echo "static const char PROGMEM SCAN_HTML[] = {" >> ../src/ESP32_WebContent.h
cat scan.html | gzip -9 | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP32_WebContent.h
cat scan.min.js.gz | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP8266_WebContent.h
cat rx_index.html | gzip -9 | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h
echo "" >> ../src/ESP8266_WebContent.h
echo "static const char PROGMEM SCAN_HTML[] = {" >> ../src/ESP8266_WebContent.h
cat scan.html | gzip -9 | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP8266_WebContent.h
cat scan.min.js.gz | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h

rm -f scan.html tx_index.html rx_index.html main.min.* scan.min.*
