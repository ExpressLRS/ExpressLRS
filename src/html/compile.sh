#!/bin/bash
#
# Make sure you install css-html-js-minify
# pip install css-html-js-minify
#
css-html-js-minify --quiet .

echo "static const char PROGMEM FLAG[] = {" > ../src/flag_svg.h
cat flag.svg | gzip -9n | xxd -i >> ../src/flag_svg.h
echo "};" >> ../src/flag_svg.h

echo "static const char PROGMEM CSS[] = {" > ../src/main_css.h
cat main.min.css | gzip -9n | xxd -i >> ../src/main_css.h
echo "};" >> ../src/main_css.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP32_WebContent.h
cat tx_index.html | gzip -9n | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h
echo "" >> ../src/ESP32_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP32_WebContent.h
cat scan.min.js | gzip -9n | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP8266_WebContent.h
cat rx_index.html | gzip -9n | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h
echo "" >> ../src/ESP8266_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP8266_WebContent.h
cat scan.min.js | gzip -9n | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h

rm -f scan.html tx_index.html rx_index.html main.min.* scan.min.*
