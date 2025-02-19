BOARD=esp32doit-devkit-v1
#BOARD=heltec_wifi_lora_32
#BOARD=nodemcu-32s
VERBOSE=1
MONITOR_SPEED=115200

GIT_VERSION := "$(shell git describe --abbrev=8 --dirty --always --tags)"
BUILD_EXTRA_FLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"

backtrace:
	tr ' ' '\n' | /home/jim/.arduino15/packages/esp32/tools/xtensa-esp32-elf-gcc/*/bin/xtensa-esp32-elf-addr2line -f -i -e ${BUILD_DIR}/${MAIN_NAME}.elf
	
CHIP=esp32
OTA_ADDR=192.168.4.154
IGNORE_STATE=1

include ${HOME}/Arduino/libraries/makeEspArduino/makeEspArduino.mk

fixtty:
	stty -F ${UPLOAD_PORT} -hupcl -crtscts -echo raw  ${MONITOR_SPEED}

cat:	fixtty
	cat ${UPLOAD_PORT}

socat:  
	socat udp-recv:9000 - 
mocat:
	mosquitto_sub -h 192.168.4.1 -t "lowpow/#" -F "%I %t %p"   

curl: ${BUILD_DIR}/${MAIN_NAME}.bin
	curl -v --limit-rate 10k --progress-bar -F "image=@${BUILD_DIR}/${MAIN_NAME}.bin" ${OTA_ADDR}/update  > /dev/null


${MAIN_NAME}_csim:	${MAIN_NAME}.ino ${HOME}/Arduino/libraries/jimlib/src/jimlib.h ${HOME}/Arduino/libraries/jimlib/src/ESP32sim_ubuntu.h
	g++  -DGIT_VERSION=\"$(GIT_VERSION)\" -x c++ -g $< -o $@ -DESP32 -DUBUNTU -I./ -I${HOME}/Arduino/libraries/jimlib/src 

csim: ${MAIN_NAME}_csim 

