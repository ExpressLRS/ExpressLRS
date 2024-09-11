#!/bin/bash

# Versioning info: 
# https://www.expresslrs.org/quick-start/receivers/firmware-version/#receiver-firmware-version 
# https://semver.org/
MAJOR_VERSION=3
MINOR_VERSION=4
PATCH_VERSION=3
export MODALAI_VERSION="0x00"
export ELRS_VER="0x0${MAJOR_VERSION}0${MINOR_VERSION}0${PATCH_VERSION}00"
ENCRYPT=0
TARGET=""
ENCRYPT_KEY=""

print_usage () {
	echo ""
	echo "Build script for buidling ExpressLRS firmware"
    echo "Args:"
    echo -e "   e) Enable encrpytion of firmware (pass in the key)"
    echo -e "   t) Target to build (m0139, r9mini)"
    echo -e "   v) Version of firmware being built"
}

while getopts "e:t:v:" opt; do
    case $opt in
        "h")
            print_usage
            exit 0
            ;;
        "e")
            ENCRYPT_KEY=${OPTARG}
            ENCRYPT=1
            echo "Using encryption"
            ;;
        "v")
            MODALAI_VERSION=${OPTARG}
            echo "Using version #: $MODALAI_VERSION"
            ;;
        "t") 
            TARGET=${OPTARG}
            echo "Building target: $TARGET"
            ;;
        *)
            echo "invalid option $arg"
            print_usage
            exit 1
            ;;
    esac
done

case $TARGET in
    "m0139"|"M0139")
        TARGET="MODALAI_M0139_via_UART"
        FW="ModalAI_M0139-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "m0139_tx"|"M0139_TX")
        TARGET="MODALAI_M0139_TX_via_UART"
        FW="ModalAI_M0139_TX-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "r9mini"|"R9Mini")
        # R9Mini support from us stops at fw version 3.2.1... last version from ModalAI was 3.2.1.3
        TARGET="MODALAI_Frsky_RX_R9MM_R9MINI_via_UART"
        MINOR_VERSON=2
        PATCH_VERSION=1
        FW="FrSky_R9Mini-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    "iFlight")
        TARGET="Unified_ESP32_900_TX_via_UART"
        FW="iFlight-900-$MAJOR_VERSION.$MINOR_VERSION.$PATCH_VERSION.$MODALAI_VERSION.bin"
        ;;
    *)
        if [[ $TARGET == "" ]]; then 
            echo "Missing Target to build for!"
        else 
            echo "Unknown target provided: ${TARGET}"
        fi
        print_usage
        exit 
        ;;
esac

# Build application
platformio run --environment $TARGET # -v > build_output.txt

BUILD_DIR=".pio/build/$TARGET"
md5sum $BUILD_DIR/firmware.bin

if [ "$ENCRYPT" -eq 1 ]; then 
    # Encrypt application 
    echo "Encrpyting application"
    stm32-encrypt $BUILD_DIR/firmware.bin $BUILD_DIR/$FW $ENCRYPT_KEY
else
    echo "Leaving application un-encrypted"
    cp $BUILD_DIR/firmware.bin $BUILD_DIR/$FW
fi

# Print out md5sum so we can verify what we push onto target matches what we just built and ecnrypted here
md5sum $BUILD_DIR/$FW

# Copy/Push ELRS FW bin to desired location on voxl2
# adb push $BUILD_DIR/$FW /usr/share/modalai/voxl-elrs/firmware/rx

cp $BUILD_DIR/$FW .
