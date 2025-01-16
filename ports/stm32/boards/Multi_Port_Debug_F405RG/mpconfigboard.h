// The Sparkfun MicroMod spec uses a zero-based peripheral numbering scheme.
// In cases where the 0th peripheral is the default, the "0" is omitted from
// the name (e.g. "I2C" instead of "I2C0").
//
// Note: UART (UART0) is not present in the edge connector pinout because the
// primary debug serial port is exposed as a virtual serial port over USB,
// i.e. Serial.print() should print over USB VCP, not UART_TX1.
//
// For more details, see https://www.sparkfun.com/micromod#tech-specs

#define MICROPY_HW_BOARD_NAME "STM32 MicroMod Processor"
#define MICROPY_HW_MCU_NAME "STM32F405RG"

// 1 = use STM32 internal flash (1 MByte)
// 0 = use onboard external SPI flash (4 MByte)
#define MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE (0)

#define MICROPY_HW_HAS_FLASH (1)
#define MICROPY_HW_ENABLE_RNG (1)
// #define MICROPY_HW_ENABLE_RTC (1)
// #define MICROPY_HW_ENABLE_DAC (1)
#define MICROPY_HW_ENABLE_USB (1)

// External SPI Flash config
#if !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE

#define MICROPY_HW_QSPIFLASH        P25Q32

#define MICROPY_HW_FLASH_FS_LABEL "STMPYBFLASH_P25Q32"


// 32 Mbit (4 MByte) external SPI flash
#define MICROPY_HW_SPIFLASH_SIZE_BITS (32 * 1024 * 1024)

#ifdef MICROPY_HW_QSPIFLASH
#define CONF_USB_MSC_LUN0_CAPACITY (MICROPY_HW_SPIFLASH_SIZE_BITS / 8)
#endif
#define MICROPY_HW_SPIFLASH_CS (pin_A4)
#define MICROPY_HW_SPIFLASH_SCK (pin_A5)
#define MICROPY_HW_SPIFLASH_MOSI (pin_A7)
#define MICROPY_HW_SPIFLASH_MISO (pin_A6)

#define MICROPY_BOARD_EARLY_INIT board_early_init
void board_early_init(void);

extern const struct _mp_spiflash_config_t spiflash_config;
extern struct _spi_bdev_t spi_bdev;
#define MICROPY_HW_SPIFLASH_ENABLE_CACHE (1)
#define MICROPY_HW_BDEV_SPIFLASH (&spi_bdev)
#define MICROPY_HW_BDEV_SPIFLASH_CONFIG (&spiflash_config)
#define MICROPY_HW_BDEV_SPIFLASH_SIZE_BYTES (MICROPY_HW_SPIFLASH_SIZE_BITS / 8)
#define MICROPY_HW_BDEV_SPIFLASH_EXTENDED (&spi_bdev) // for extended block protocol

#endif // !MICROPY_HW_ENABLE_INTERNAL_FLASH_STORAGE


// STM32 HSE config
// The module has a 8 MHz crystal for the HSE oscillator.
#define MICROPY_HW_CLK_PLLM (8)
#define MICROPY_HW_CLK_PLLN (336)
#define MICROPY_HW_CLK_PLLP (RCC_PLLP_DIV2)
#define MICROPY_HW_CLK_PLLQ (7)
#define MICROPY_HW_CLK_LAST_FREQ (1)

// STM32 LSE config
// The module has a 32.768 kHz crystal for the LSE (RTC).
// #define MICROPY_HW_RTC_USE_LSE (1)
// #define MICROPY_HW_RTC_USE_US (0)
// #define MICROPY_HW_RTC_USE_CALOUT (1)


// UART1 config (MicroMod UART1)
#define MICROPY_HW_UART1_NAME "UART1"
#define MICROPY_HW_UART1_TX (pin_A9)
#define MICROPY_HW_UART1_RX (pin_A10)


// UART2 config (MicroMod UART2)
#define MICROPY_HW_UART2_NAME "UART2"
#define MICROPY_HW_UART2_TX (pin_A2)
#define MICROPY_HW_UART2_RX (pin_A3)

// UART3 config (MicroMod UART3)
#define MICROPY_HW_UART3_NAME "UART3"
#define MICROPY_HW_UART3_TX (pin_B10)
#define MICROPY_HW_UART3_RX (pin_B11)

// UART4 config (MicroMod UART4)
#define MICROPY_HW_UART4_NAME "UART4"
#define MICROPY_HW_UART4_TX (pin_A0)
#define MICROPY_HW_UART4_RX (pin_A1)


// UART5 config (MicroMod UART5)
#define MICROPY_HW_UART5_NAME "UART5"
#define MICROPY_HW_UART5_TX (pin_C12)
#define MICROPY_HW_UART5_RX (pin_D2)


// UART4 config (MicroMod UART4)
#define MICROPY_HW_UART6_NAME "UART6"
#define MICROPY_HW_UART6_TX (pin_C6)
#define MICROPY_HW_UART6_RX (pin_C7)


// CAN1 config (MicroMod CAN)
#define MICROPY_HW_CAN1_NAME "CAN1"
#define MICROPY_HW_CAN1_TX (pin_A12)
#define MICROPY_HW_CAN1_RX (pin_A11)


// CAN1 config (MicroMod CAN)
#define MICROPY_HW_CAN2_NAME "CAN2"
#define MICROPY_HW_CAN2_TX (pin_B13)
#define MICROPY_HW_CAN2_RX (pin_B12)

// // I2C1 config (MicroMod I2C1)
// #define MICROPY_HW_I2C1_NAME "I2C1"
// #define MICROPY_HW_I2C1_SCL (pin_B6)
// #define MICROPY_HW_I2C1_SDA (pin_B7)

// I2C2 config (MicroMod I2C)
#define MICROPY_HW_I2C3_NAME "I2C3"
#define MICROPY_HW_I2C3_SCL (pin_A8)
#define MICROPY_HW_I2C3_SDA (pin_C9)

// // SPI1 config (MicroMod SPI)
// #define MICROPY_HW_SPI1_NAME "SPI"
// #define MICROPY_HW_SPI1_NSS (pin_C4)
// #define MICROPY_HW_SPI1_SCK (pin_A5)
// #define MICROPY_HW_SPI1_MISO (pin_A6)
// #define MICROPY_HW_SPI1_MOSI (pin_A7)

// LED1 config
// The module has a single blue status LED.
#define MICROPY_HW_LED1 (pin_C15)
#define MICROPY_HW_LED_ON(pin) (mp_hal_pin_high(pin))
#define MICROPY_HW_LED_OFF(pin) (mp_hal_pin_low(pin))

// USB device config
#define MICROPY_HW_USB_HS (1)
#define MICROPY_HW_USB_HS_IN_FS             (1)
