#!/bin/bash


# chmod +x py_env.sh
make -C mpy-cross

cd ports/stm32
BOARD=Multi_Port_Debug_F407ZE 
# rm -rf build-Multi_Port_Debug_F407ZE
# make BOARD=$BOARD  submodules
make BOARD=$BOARD  LTO=1
cp build-Multi_Port_Debug_F407ZE/firmware.hex boards/Multi_Port_Debug_F407ZE/
# cp build-Multi_Port_Debug_F407ZE/firmware0.bin boards/Multi_Port_Debug_F407ZE/
# cp build-Multi_Port_Debug_F407ZE/firmware1.bin boards/Multi_Port_Debug_F407ZE/


