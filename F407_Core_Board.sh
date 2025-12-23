#!/bin/bash
# chmod +x py_env.sh
BOARD=F407_Core_Board 
make -C mpy-cross
cd ports/stm32

rm -rf build-${BOARD}




# make BOARD=${BOARD} LTO=1 

make BOARD=${BOARD} LTO=1
FIRMWARE_DIR=boards/${BOARD}
mkdir -p ${FIRMWARE_DIR}
cp build-${BOARD}/firmware.hex ${FIRMWARE_DIR}
# cp build-${BOARD}/firmware0.bin ${FIRMWARE_DIR}
# cp build-${BOARD}/firmware1.bin ${FIRMWARE_DIR}