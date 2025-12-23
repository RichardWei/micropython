#!/bin/bash

# chmod +x py_env.sh

BOARD=Multi_Port_Debug_F407ZE
make -C mpy-cross
cd ports/stm32

# rm -rf build-${BOARD}

# 构造目录名
DIR_NAME="build-${BOARD}"

# 检查目录是否存在
if [ -d "$DIR_NAME" ]; then
    # 如果目录存在，删除它
    rm -rf "$DIR_NAME"
    echo "dir $DIR_NAME delete"
else
    # 如果目录不存在，输出信息
    echo "dir $DIR_NAME don't exist"
fi


# make BOARD=${BOARD} LTO=1 

make BOARD=${BOARD} LTO=1 -j8
FIRMWARE_DIR=boards/${BOARD}
mkdir -p ${FIRMWARE_DIR}
cp build-${BOARD}/firmware.hex ${FIRMWARE_DIR}
# cp build-${BOARD}/firmware0.bin ${FIRMWARE_DIR}
# cp build-${BOARD}/firmware1.bin ${FIRMWARE_DIR}
