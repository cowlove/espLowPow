#!/bin/bash 
touch *.ino
make
ssh ls 'rm simplePost/firmware.bin simplePost/firmware.ver'
strings /tmp/mkESP/espLowPow_esp32doit-devkit-v1/espLowPow_.cpp.o | grep GIT_VER |
     rematch '"([0-9a-fA-F]+)"' > firmware.ver
scp /tmp/mkESP/espLowPow_esp32doit-devkit-v1/espLowPow.bin ls:simplePost/firmware.bin
scp firmware.ver ls:simplePost/firmware.ver
