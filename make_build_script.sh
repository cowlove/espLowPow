#!/bin/bash
TMP=/tmp/arduino/$$.txt
PORT=/dev/ttyACM0
cd `dirname $0`
arduino-cli compile  -v \
    -b esp32:esp32:esp32c6 --board-options PartitionScheme=min_spiffs,CDCOnBoot=cdc -e -u --port ${PORT} \
    | tee $TMP

SKETCH=`basename \`pwd\``
OUT=./build.sh
SKETCHDIR=/tmp/arduino/sketches/`ls -1tr /tmp/arduino/sketches/ | tail -1`

echo "#!/bin/bash" > $OUT
echo "echo '#include <Arduino.h>' > ${SKETCHDIR}/sketch/${SKETCH}.ino.cpp" >> $OUT
echo "cat espLowPow.ino >> ${SKETCHDIR}/sketch/${SKETCH}.ino.cpp" >> $OUT

egrep "${SKETCH}.ino.cpp.o" $TMP >> $OUT
egrep '^python3' $TMP >> $OUT
grep esptool_py $TMP | grep -v ${PORT} >> $OUT
echo "echo Waiting for ${PORT}...; while [ ! -e ${PORT} ]; do sleep 1; done" >> $OUT
grep esptool_py $TMP | grep ${PORT} >> $OUT
echo "echo Waiting for ${PORT}...; while [ ! -e ${PORT} ]; do sleep 1; done" >> $OUT
echo "stty -F ${PORT} raw -echo && cat ${PORT}" >> $OUT

mkdir build
cp -a "${SKETCHDIR}/${SKETCH}"* build/
chmod 755 $OUT
#rm -f $TMP