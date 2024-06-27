#!/bin/bash


# chmod +x py_env.sh

make -C mpy-cross


cd ports/stm32

BOARD=VCC_GND_F407ZG 


# make BOARD=$BOARD  submodules

# make BOARD=$BOARD  MICROPY_PY_NETWORK_WIZNET5K=5500


# rm -rf build-VCC_GND_F407ZG
# make BOARD=$BOARD  submodules
make BOARD=$BOARD  LTO=1
cp build-VCC_GND_F407ZG/firmware.hex boards/VCC_GND_F407ZG/
