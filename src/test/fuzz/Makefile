# RX fuzzer. Usage: make run [SEED=n COUNT=n] | make repro SEED=n | make run FUZZ_SKIP=leak:SBUS,blackout
SRC := ../..
CXX := clang++
# physical cores, nproc counts hyperthreads
JOBS ?= $(shell lscpu -p=core | sort -u | grep -c '^[0-9]')
MAKEFLAGS += -j$(JOBS)
SEED ?= random
COUNT ?= 1000000

LIBDIRS := $(wildcard $(SRC)/lib/*/)
CXXFLAGS := -std=c++17 -g -O1 -w -fsanitize=address,undefined -fno-sanitize=shift-base -fno-sanitize-recover=undefined \
	-DTARGET_NATIVE -DTARGET_RX -DRADIO_SX128X=1 -DPROGMEM= \
	-Istubs -include stubs/arduino_stubs.h -include $(SRC)/include/target/Unified_ESP_RX.h \
	-I$(SRC)/include $(addprefix -I,$(LIBDIRS)) -I$(SRC)/src -I$(SRC)/src/rx-serial

# The firmware as built for an RX, minus what sim_hardware.cpp replaces: radio driver, hardware timer,
# EEPROM, options and the device modules that need real peripherals
FIRMWARE := src/rx_main.cpp src/common.cpp src/rxtx_common.cpp src/dynpower.cpp \
	src/rx-serial/devSerialIO.cpp src/rx-serial/SerialIO.cpp src/rx-serial/SerialCRSF.cpp \
	src/rx-serial/SerialSBUS.cpp src/rx-serial/SerialSUMD.cpp src/rx-serial/SerialAirPort.cpp \
	src/rx-serial/SerialDisplayport.cpp src/rx-serial/SerialScorpion_TLM.cpp \
	src/rx-serial/SerialGPS.cpp src/rx-serial/SerialGPS-nmea.cpp src/rx-serial/SerialGPS-ublox.cpp \
	lib/OTA/OTA.cpp lib/CRC/crc.cpp lib/CONFIG/config.cpp lib/DEVICE/device.cpp \
	lib/FHSS/FHSS.cpp lib/FHSS/random.cpp lib/logging/logging.cpp lib/POWERMGNT/POWERMGNT.cpp \
	lib/BUTTON/devButton.cpp lib/LED/devLED.cpp \
	lib/StubbornReceiver/stubborn_receiver.cpp lib/StubbornSender/stubborn_sender.cpp \
	lib/CrsfProtocol/CRSFConnector.cpp lib/CrsfProtocol/CRSFEndpoint.cpp lib/CrsfProtocol/CRSFParser.cpp \
	lib/CrsfProtocol/CRSFRouter.cpp lib/CrsfProtocol/RxTxEndpoint.cpp \
	lib/rx-crsf/devRXLUA.cpp lib/rx-crsf/RXEndpoint.cpp lib/rx-crsf/RXOTAConnector.cpp lib/rx-crsf/RXParameters.cpp
HARNESS := fuzz_harness.cpp fuzz_channel.cpp fuzz_checks.cpp sim_tx.cpp sim_hardware.cpp fuzz_main.cpp

OBJS := $(FIRMWARE:%.cpp=build/fw/%.o) $(HARNESS:%.cpp=build/%.o) build/tx_ota.o
HDRS := Makefile $(wildcard *.h stubs/*.h $(SRC)/include/*.h $(SRC)/include/target/*.h $(SRC)/lib/*/*.h $(SRC)/src/rx-serial/*.h)

build/rx_fuzzer: $(OBJS)
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $@

# The simulated TX gets its own copy of OTA.cpp, built with the packer. Everything in the copy except
# the txOta*() functions is made local, so its globals are not the RX's.
build/tx/%.o: CXXFLAGS += -DTARGET_TX -fvisibility=hidden
build/tx/OTA.o: $(SRC)/lib/OTA/OTA.cpp $(HDRS)
build/tx/sim_tx_ota.o: sim_tx_ota.cpp $(HDRS)
build/tx/%.o:
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

build/tx_ota.o: build/tx/OTA.o build/tx/sim_tx_ota.o
	@ld -r --force-group-allocation $^ -o $@
	@objcopy --localize-hidden $@

build/fw/%.o: $(SRC)/%.cpp $(HDRS)
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: %.cpp $(HDRS)
	@mkdir -p $(dir $@)
	@echo "CXX $<"
	@$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: run repro clean
run: build/rx_fuzzer
	@FUZZ_SKIP=$(FUZZ_SKIP) build/rx_fuzzer $(SEED) $(COUNT) $(JOBS); true

repro: build/rx_fuzzer
	@FUZZ_TRACE=1 FUZZ_SKIP=$(FUZZ_SKIP) build/rx_fuzzer $(SEED) 1; true

clean:
	rm -rf build
