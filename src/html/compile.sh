#!/bin/bash
#
# Make sure you install css-html-js-minify
# pip install css-html-js-minify
#
cd "$(dirname "$0")"

css-html-js-minify --quiet .

#!/bin/sh

GITBRANCH=`git describe --tags --exact-match 2> /dev/null || git symbolic-ref -q --short HEAD`
GITHASH=`git rev-parse --short HEAD`
VERSION="$GITBRANCH ($GITHASH)"

echo "static const char PROGMEM FLAG[] = {" > ../src/flag_svg.h
cat flag.svg | gzip -9n | xxd -i >> ../src/flag_svg.h
echo "};" >> ../src/flag_svg.h

echo "static const char PROGMEM CSS[] = {" > ../src/main_css.h
cat main.min.css | gzip -9n | xxd -i >> ../src/main_css.h
echo "};" >> ../src/main_css.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP32_WebContent.h
cat tx_index.html | sed "s/@VERSION@/$VERSION/" | gzip -9n | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h
echo "" >> ../src/ESP32_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP32_WebContent.h
cat scan.min.js | gzip -9n | xxd -i >> ../src/ESP32_WebContent.h
echo "};" >> ../src/ESP32_WebContent.h

echo "static const char PROGMEM INDEX_HTML[] = {" > ../src/ESP8266_WebContent.h
cat rx_index.html | sed "s/@VERSION@/$VERSION/" | gzip -9n | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h
echo "" >> ../src/ESP8266_WebContent.h
echo "static const char PROGMEM SCAN_JS[] = {" >> ../src/ESP8266_WebContent.h
cat scan.min.js | gzip -9n | xxd -i >> ../src/ESP8266_WebContent.h
echo "};" >> ../src/ESP8266_WebContent.h

rm -f scan.html tx_index.html rx_index.html main.min.* scan.min.*
