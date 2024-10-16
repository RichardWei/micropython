



MCU_SERIES = f4
CMSIS_MCU = STM32F407xx
AF_FILE = boards/stm32f407_af.csv
ifeq ($(USE_MBOOT),1)
# When using Mboot all the text goes together after the filesystem
LD_FILES = boards/stm32f407ze.ld boards/common_blifs.ld
TEXT0_ADDR = 0x08020000
else
# When not using Mboot the ISR text goes first, then the rest after the filesystem
LD_FILES = boards/stm32f407ze.ld boards/common_ifs.ld
TEXT0_ADDR = 0x08000000
TEXT1_ADDR = 0x08001000
endif

# MicroPython settings
MBOOT_VFS_FAT =1
MICROPY_VFS_LFS2 = 1
MICROPY_PY_LWIP = 1
MICROPY_PY_SSL = 1
MICROPY_SSL_MBEDTLS = 1

# CFLAGS += -Os -s
MICROPY_HW_ENABLE_ISR_UART_FLASH_FUNCS_IN_RAM = 1

# Provide different variants for the downloads page.
ifeq ($(BOARD_VARIANT),DP)
MICROPY_FLOAT_IMPL=double
endif

ifeq ($(BOARD_VARIANT),THREAD)
CFLAGS += -DMICROPY_PY_THREAD=1
endif

ifeq ($(BOARD_VARIANT),DP_THREAD)
MICROPY_FLOAT_IMPL=double
CFLAGS += -DMICROPY_PY_THREAD=1
endif

ifeq ($(BOARD_VARIANT),NETWORK)
MICROPY_PY_NETWORK_WIZNET5K=5200
endif

# PYB-specific frozen modules
FROZEN_MANIFEST = $(BOARD_DIR)/manifest.py