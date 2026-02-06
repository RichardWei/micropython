import time
from machine import Pin
from swi import SWI
import time
SWI_PIN = "C9"
ON_DEFAULT_ADDRESS = 0x30
ON_UID_ADDR = 0x64

swi = SWI(Pin(SWI_PIN), strong_pullup=True)

# ok.txt 对齐时序
swi.set_timing(505, 1653, 2700, 12000)

swi.powerup();   time.sleep_ms(2282)
swi.powerdown(); time.sleep_ms(3)
swi.powerup();   time.sleep_ms(13)

swi.wakeup()
swi.reset_delay()
swi.powercycle()

print("Start capture now, sending in 2 seconds...")
# time.sleep_ms(2000)

# EDA/SDA
swi.send_raw_word_no_irq(0x09, 0x00)   # EDA(0)
swi.send_raw_word_no_irq(0x0A, 0x30)   # SDA(0x30)

# RBL3
swi.send_rbl(3)

# ERA + RRA(0x64, 8 bytes)
swi.send_era((ON_UID_ADDR >> 8) & 0xFF)
uid = bytearray()
uid += swi.send_rra(ON_UID_ADDR & 0xFF, 8)

# RBL2 + ERA + RRA(0x66, 4 bytes)
swi.send_rbl(2)
swi.send_era(((ON_UID_ADDR + 2) >> 8) & 0xFF)
uid += swi.send_rra((ON_UID_ADDR + 2) & 0xFF, 4)

print("UID:", uid.hex())