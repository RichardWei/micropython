#!/bin/bash


# chmod +x py_env.sh

make -C mpy-cross


cd ports/stm32

BOARD=F407_Core_Board 


# make BOARD=$BOARD  submodules

# make BOARD=$BOARD  MICROPY_PY_NETWORK_WIZNET5K=5500


rm -rf build-F407_Core_Board
# make BOARD=$BOARD  submodules
make BOARD=$BOARD  LTO=1
cp build-F407_Core_Board/firmware.hex boards/F407_Core_Board/