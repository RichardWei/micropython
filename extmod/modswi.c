/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2026
 */

#include <stdint.h>

#include "py/obj.h"
#include "py/mperrno.h"
#include "py/mphal.h"
#include "py/runtime.h"


#if MICROPY_PY_SWI

// Low-speed defaults from SDK (in microseconds)
#define SWI_DEFAULT_TAU1_US     (90)
#define SWI_DEFAULT_TAU3_US     (310)
#define SWI_DEFAULT_TAU5_US     (400)
#define SWI_DEFAULT_TIMEOUT_US  (3000)

// Wakeup/reset timing defaults from SDK (microseconds)
#define SWI_WAKE_DELAY_US       (7000)
#define SWI_WAKE_LOW_US         (10)
#define SWI_WAKE_FILTER_US      (28)
#define SWI_RESET_DELAY_US      (1000)
#define SWI_POWERDOWN_US        (2000)
#define SWI_POWERUP_US          (8000)
// Debounce after mode switch to let the line settle (microseconds)
#define SWI_DEBOUNCE_US         (0)

// Parasitic power charging time (Logic 1 High duration)
// Defaulting to TAU1 as minimum charging window
#define SWI_TCHARGE_US          (SWI_DEFAULT_TAU1_US)

// Training-bit validation (matches SDK default)
#ifndef MICROPY_SWI_TRAINING_CHECK
#define MICROPY_SWI_TRAINING_CHECK (1)
#endif

// SWI command codes (subset)
#define SWI_BC   (0x08)
#define SWI_WD   (0x04)
#define SWI_ERA  (0x05)
#define SWI_RRA  (0x07)
#define SWI_BRES (0x00)
#define SWI_PDWN (0x02)
// RBLn (read burst length 2^n)
#define SWI_RBL0 (0x20)
#define SWI_RBL1 (0x21)
#define SWI_RBL2 (0x22)
#define SWI_RBL3 (0x23)

static uint32_t swi_tau1_us = SWI_DEFAULT_TAU1_US;
static uint32_t swi_tau3_us = SWI_DEFAULT_TAU3_US;
static uint32_t swi_tau5_us = SWI_DEFAULT_TAU5_US;
static uint32_t swi_timeout_us = SWI_DEFAULT_TIMEOUT_US;
static uint32_t swi_wake_delay_us = SWI_WAKE_DELAY_US;
static uint32_t swi_wake_low_us = SWI_WAKE_LOW_US;
static uint32_t swi_wake_filter_us = SWI_WAKE_FILTER_US;
static uint32_t swi_reset_delay_us = SWI_RESET_DELAY_US;
static uint32_t swi_powerdown_us = SWI_POWERDOWN_US;
static uint32_t swi_powerup_us = SWI_POWERUP_US;

// Helper to switch to Open-Drain / Input (Release Bus)
static inline void swi_pin_input_od(mp_hal_pin_obj_t pin) {
    // Release the line for input (OD/high-Z)
    mp_hal_pin_input(pin);
}

// Helper to switch to Push-Pull Output (Drive Bus)
static inline void swi_pin_output_pp(mp_hal_pin_obj_t pin) {
    // SDK Alignment: Use mp_hal_pin_output to strictly match GPIO_set_dir(OUT)
    mp_hal_pin_output(pin);
}

// Helper to drive value
static inline void swi_pin_drive(mp_hal_pin_obj_t pin, int value) {
    if (value) {
        mp_hal_pin_high(pin);
    } else {
        mp_hal_pin_low(pin);
    }
}

static void swi_bus_stop(mp_hal_pin_obj_t pin, bool strong_pullup) {
    // STOP bit: Logic 1 (High) for Tau5, then release to input
    if (strong_pullup) {
        swi_pin_output_pp(pin);
        swi_pin_drive(pin, 1);
    } else {
        mp_hal_pin_od_high(pin);
    }
    mp_hal_delay_us(swi_tau5_us);
    // Force OD-high before input to guarantee release after push-pull
    mp_hal_pin_open_drain(pin);
    mp_hal_pin_od_high(pin);
    swi_pin_input_od(pin);
}

static void swi_bus_release(mp_hal_pin_obj_t pin) {
    // Ensure bus is released (OD-high -> input) to avoid push-pull hold
    mp_hal_pin_open_drain(pin);
    mp_hal_pin_od_high(pin);
    swi_pin_input_od(pin);
    mp_hal_delay_us(SWI_DEBOUNCE_US);
}

static void swi_bus_wakeup(mp_hal_pin_obj_t pin, bool strong_pullup) {
    // SDK: pull low for wake_delay + wake_low, then high for wake_filter
    if (strong_pullup) {
        swi_pin_output_pp(pin);
        swi_pin_drive(pin, 0);
        mp_hal_delay_us(swi_wake_delay_us + swi_wake_low_us);
        swi_pin_drive(pin, 1);
        mp_hal_delay_us(swi_wake_filter_us);
        swi_pin_input_od(pin);
    } else {
        mp_hal_pin_od_low(pin);
        mp_hal_delay_us(swi_wake_delay_us + swi_wake_low_us);
        mp_hal_pin_od_high(pin);
        mp_hal_delay_us(swi_wake_filter_us);
    }
}

static void swi_bus_reset_delay(void) {
    mp_hal_delay_us(swi_reset_delay_us);
}

static void swi_bus_powerdown(mp_hal_pin_obj_t pin, bool strong_pullup) {
    if (strong_pullup) {
        swi_pin_output_pp(pin);
        swi_pin_drive(pin, 0);
    } else {
        mp_hal_pin_od_low(pin);
    }
}

static void swi_bus_powerup(mp_hal_pin_obj_t pin, bool strong_pullup) {
    if (strong_pullup) {
        swi_pin_output_pp(pin);
        swi_pin_drive(pin, 1);
        mp_hal_delay_us(swi_powerup_us);
        swi_pin_input_od(pin);
    } else {
        mp_hal_pin_od_high(pin);
        mp_hal_delay_us(swi_powerup_us);
    }
}

static void swi_bus_powerdown_hardrst(mp_hal_pin_obj_t pin, bool strong_pullup) {
    if (strong_pullup) {
        swi_pin_output_pp(pin);
        swi_pin_drive(pin, 0);
        mp_hal_delay_us(swi_powerdown_us);
        swi_pin_drive(pin, 1);
        mp_hal_delay_us(swi_powerup_us);
        swi_pin_input_od(pin);
    } else {
        mp_hal_pin_od_low(pin);
        mp_hal_delay_us(swi_powerdown_us);
        mp_hal_pin_od_high(pin);
        mp_hal_delay_us(swi_powerup_us);
    }
}

static void swi_bus_send_raw_word(mp_hal_pin_obj_t pin, uint32_t raw17, bool wait_irq, bool immediate_irq, bool strong_pullup) {
    // STOP first (release bus)
    swi_bus_stop(pin, strong_pullup);

    uint32_t masks[17] = {
        0x10000, 0x08000, 0x04000, 0x02000, 0x01000, 0x00800, 0x00400,
        0x00200, 0x00100, 0x00080, 0x00040, 0x00020, 0x00010, 0x00008,
        0x00004, 0x00002, 0x00001,
    };

    // Configure output mode once for the whole word
    if (strong_pullup) {
        swi_pin_output_pp(pin);
    } else {
        mp_hal_pin_open_drain(pin);
    }

    uint32_t quiet = mp_hal_quiet_timing_enter();
    int level = 0;
    for (int i = 0; i < 17; ++i) {
        if (strong_pullup) {
            swi_pin_drive(pin, level);
        } else {
            if (level) {
                mp_hal_pin_od_high(pin);
            } else {
                mp_hal_pin_od_low(pin);
            }
        }

        if (raw17 & masks[i]) {
            mp_hal_delay_us_fast(swi_tau3_us);
        } else {
            mp_hal_delay_us_fast(swi_tau1_us);
        }
        level ^= 1;
    }
    mp_hal_quiet_timing_exit(quiet);

    // STOP and release to input before any response
    swi_bus_stop(pin, strong_pullup);

    if (wait_irq || immediate_irq) {
        uint32_t start = mp_hal_ticks_us();
        while (mp_hal_pin_read(pin)) {
            if (mp_hal_ticks_us() - start > swi_timeout_us) {
                break;
            }
        }
        mp_hal_delay_us(swi_tau1_us);
        mp_hal_delay_us(swi_tau5_us);
    }
}

static uint32_t swi_bus_recv_raw_word(mp_hal_pin_obj_t pin) {
    // Ensure line is released before sampling (Input mode)
    swi_bus_release(pin);
    
    uint32_t durations[17];
    uint32_t start = mp_hal_ticks_us();
    while (mp_hal_pin_read(pin)) {
        if (mp_hal_ticks_us() - start > swi_timeout_us) {
            mp_raise_OSError(MP_ETIMEDOUT);
        }
    }

    int prev = mp_hal_pin_read(pin);
    uint32_t last = mp_hal_ticks_us();
    uint32_t quiet = mp_hal_quiet_timing_enter();
    for (int i = 16; i >= 0; --i) {
        start = mp_hal_ticks_us();
        while (mp_hal_pin_read(pin) == prev) {
            if (mp_hal_ticks_us() - start > swi_timeout_us) {
                mp_hal_quiet_timing_exit(quiet);
                mp_raise_OSError(MP_ETIMEDOUT);
            }
        }
        uint32_t now = mp_hal_ticks_us();
        durations[i] = now - last;
        last = now;
        prev = mp_hal_pin_read(pin);
    }
    mp_hal_quiet_timing_exit(quiet);

    // Match SDK: compute threshold from bits 16..1 (exclude index 0)
    uint32_t min_d = durations[16];
    uint32_t max_d = durations[16];
    for (int i = 15; i >= 1; --i) {
        if (durations[i] < min_d) {
            min_d = durations[i];
        }
        if (durations[i] > max_d) {
            max_d = durations[i];
        }
    }
    uint32_t threshold = min_d + ((max_d - min_d) >> 1);

    uint32_t raw = 0;
    for (int i = 0; i < 17; ++i) {
        if (durations[i] > threshold) {
            raw |= (1u << i);
        }
    }

#if MICROPY_SWI_TRAINING_CHECK
    // Training-bit validation and tau-based decode (SDK-style)
    uint32_t training0 = durations[16];
    uint32_t training1 = durations[15];
    uint32_t min_tau1 = (swi_tau1_us * 6) / 10;   // 0.6 * tau
    uint32_t max_tau1 = (swi_tau1_us * 14) / 10;  // 1.4 * tau
    uint32_t min_tau3 = (swi_tau1_us * 26) / 10;  // 2.6 * tau
    uint32_t max_tau3 = (swi_tau1_us * 34) / 10;  // 3.4 * tau

    if (training0 > training1) {
        if (training0 < min_tau3 || training0 > max_tau3) {
            mp_raise_OSError(MP_EIO);
        }
    } else {
        if (training0 < min_tau1 || training0 > max_tau1) {
            mp_raise_OSError(MP_EIO);
        }
    }

    if (training1 > training0) {
        if (training1 < min_tau3 || training1 > max_tau3) {
            mp_raise_OSError(MP_EIO);
        }
    } else {
        if (training1 < min_tau1 || training1 > max_tau1) {
            mp_raise_OSError(MP_EIO);
        }
    }

    uint32_t tau_est = (training0 + training1) >> 2;
    uint32_t tau_based = 0;
    for (int i = 0; i < 17; ++i) {
        uint32_t d = durations[i];
        uint32_t half_tau = tau_est >> 1;
        if (d >= half_tau && d <= (tau_est + half_tau)) {
            // bit 0
        } else if (d >= ((2 * tau_est) + half_tau) && d <= ((3 * tau_est) + half_tau)) {
            tau_based |= (1u << i);
        } else {
            mp_raise_OSError(MP_EIO);
        }
    }

    if (tau_based != raw) {
        raw = tau_based;
    }
#endif

    return raw;
}

static uint8_t swi_count_ones(uint16_t value, uint8_t bits) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < bits; ++i) {
        if ((value >> i) & 0x01) {
            count++;
        }
    }
    return count;
}

static uint16_t swi_compute_parity(uint16_t cmd) {
    uint16_t cmd_parity = 0;
    const uint16_t P0_MASK = 0x055b;
    const uint16_t P1_MASK = 0x066d;
    const uint16_t P2_MASK = 0x078e;
    const uint16_t P3_MASK = 0x07f0;

    cmd_parity |= (swi_count_ones(cmd & P0_MASK, 16) & 0x01);
    cmd_parity |= (swi_count_ones(cmd & P1_MASK, 16) & 0x01) << 1;
    cmd_parity |= (swi_count_ones(cmd & P2_MASK, 16) & 0x01) << 3;
    cmd_parity |= (swi_count_ones(cmd & P3_MASK, 16) & 0x01) << 7;

    cmd_parity |= ((cmd & 0xfff0) << 4) | ((cmd & 0x0e) << 3) | ((cmd & 0x01) << 2);
    return cmd_parity;
}

static uint32_t swi_add_invert_flag(uint16_t cmd_with_parity) {
    uint32_t cmd_17 = ((uint32_t)cmd_with_parity) << 1;
    if (swi_count_ones(cmd_with_parity, 14) >= 8) {
        cmd_17 |= 1;
        cmd_17 ^= 0x7ffe;
    }
    return cmd_17 & 0x1ffff;
}

static uint8_t swi_reflect8(uint8_t val) {
    uint8_t res = 0;
    for (int i = 0; i < 8; ++i) {
        if (val & (1 << i)) {
            res |= (uint8_t)(1 << (7 - i));
        }
    }
    return res;
}

static uint16_t swi_reflect16(uint16_t val) {
    uint16_t res = 0;
    for (int i = 0; i < 16; ++i) {
        if (val & (1 << i)) {
            res |= (uint16_t)(1 << (15 - i));
        }
    }
    return res;
}

static uint16_t swi_crc16_update(uint16_t crc, uint8_t data) {
    uint8_t byte = swi_reflect8(data);
    crc ^= (uint16_t)byte << 8;
    for (int b = 0; b < 8; ++b) {
        if (crc & 0x8000) {
            crc = (uint16_t)((crc << 1) ^ 0x8005);
        } else {
            crc = (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t swi_crc16_finish(uint16_t crc) {
    return (uint16_t)(swi_reflect16(crc) ^ 0xffff);
}

static void swi_send_crc(mp_hal_pin_obj_t pin, uint16_t crc, bool strong_pullup) {
    uint32_t cmd = ((uint32_t)SWI_WD << 8) | (crc & 0xff);
    cmd = swi_add_invert_flag(swi_compute_parity((uint16_t)cmd));
    swi_bus_send_raw_word(pin, cmd, false, false, strong_pullup);

    cmd = ((uint32_t)SWI_WD << 8) | ((crc >> 8) & 0xff);
    cmd = swi_add_invert_flag(swi_compute_parity((uint16_t)cmd));
    swi_bus_send_raw_word(pin, cmd, false, false, strong_pullup);
}

static void swi_send_cmd_with_crc(mp_hal_pin_obj_t pin, uint8_t code, uint8_t data, bool strong_pullup) {
    uint32_t cmd = ((uint32_t)code << 8) | data;
    cmd = swi_add_invert_flag(swi_compute_parity((uint16_t)cmd));
    swi_bus_send_raw_word(pin, cmd, false, false, strong_pullup);

    uint16_t crc = 0xffff;
    crc = swi_crc16_update(crc, cmd & 0xff);
    crc = swi_crc16_update(crc, (cmd >> 8) & 0xff);
    crc = swi_crc16_update(crc, (cmd >> 16) & 0x01);
    crc = swi_crc16_finish(crc);
    swi_send_crc(pin, crc, strong_pullup);

    // SDK behavior: each raw word already ends with STOP + input.
    // Do not add an extra STOP here; RRA expects immediate response after CRC.
}

static void swi_decode_raw_word(uint32_t raw17, uint16_t *cmd_out, bool *parity_ok) {
    const uint16_t P0_MASK = 0x5555;
    const uint16_t P1_MASK = 0x6666;
    const uint16_t P2_MASK = 0x7878;
    const uint16_t P3_MASK = 0x7f80;

    if (raw17 & 0x01) {
        raw17 ^= 0x7fff;
    }
    raw17 >>= 1;
    uint16_t cmd = (uint16_t)raw17;

    uint16_t parity = 0;
    parity |= (swi_count_ones(cmd & P0_MASK, 16) & 0x01);
    parity |= (swi_count_ones(cmd & P1_MASK, 16) & 0x01) << 1;
    parity |= (swi_count_ones(cmd & P2_MASK, 16) & 0x01) << 2;
    parity |= (swi_count_ones(cmd & P3_MASK, 16) & 0x01) << 3;
    *parity_ok = (parity == 0);

    cmd = ((cmd >> 2) & 0x01) | ((cmd >> 3) & 0x0e) | ((cmd >> 4) & 0xfff0);
    *cmd_out = cmd;
}

static void swi_receive_data(mp_hal_pin_obj_t pin, uint8_t *out, size_t len) {
    uint16_t crc = 0xffff;
    for (size_t i = 0; i < len; ++i) {
        uint32_t raw = swi_bus_recv_raw_word(pin);
        crc = swi_crc16_update(crc, raw & 0xff);
        crc = swi_crc16_update(crc, (raw >> 8) & 0xff);
        crc = swi_crc16_update(crc, (raw >> 16) & 0x01);

        uint16_t cmd;
        bool parity_ok;
        swi_decode_raw_word(raw, &cmd, &parity_ok);
        if (!parity_ok) {
            mp_raise_OSError(MP_EIO);
        }
        if ((cmd & 0x0200) != 0x0200) {
            out[i] = (uint8_t)(cmd & 0xff);
            mp_raise_OSError(MP_EIO);
        }
        out[i] = (uint8_t)(cmd & 0xff);
    }

    uint16_t crc_rx = 0;
    for (size_t i = 0; i < 2; ++i) {
        uint32_t raw = swi_bus_recv_raw_word(pin);
        uint16_t cmd;
        bool parity_ok;
        swi_decode_raw_word(raw, &cmd, &parity_ok);
        if (!parity_ok) {
            mp_raise_OSError(MP_EIO);
        }
        crc_rx |= (uint16_t)(cmd & 0xff) << (i * 8);
    }

    crc = swi_crc16_finish(crc);
    if (crc != crc_rx) {
        mp_raise_OSError(MP_EIO);
    }

    // SDK-style STOP after receive
    swi_bus_stop(pin, false);
    swi_bus_release(pin);
}

// MicroPython bindings

static mp_obj_t swi_set_timing(size_t n_args, const mp_obj_t *args) {
    (void)mp_hal_get_pin_obj(args[0]);
    swi_tau1_us = mp_obj_get_int(args[1]);
    swi_tau3_us = mp_obj_get_int(args[2]);
    swi_tau5_us = mp_obj_get_int(args[3]);
    swi_timeout_us = mp_obj_get_int(args[4]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_set_timing_obj, 5, 5, swi_set_timing);

static mp_obj_t swi_set_wakeup_timing(size_t n_args, const mp_obj_t *args) {
    (void)mp_hal_get_pin_obj(args[0]);
    swi_wake_delay_us = mp_obj_get_int(args[1]);
    swi_wake_low_us = mp_obj_get_int(args[2]);
    swi_wake_filter_us = mp_obj_get_int(args[3]);
    swi_reset_delay_us = mp_obj_get_int(args[4]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_set_wakeup_timing_obj, 5, 5, swi_set_wakeup_timing);

static mp_obj_t swi_set_power_timing(size_t n_args, const mp_obj_t *args) {
    (void)mp_hal_get_pin_obj(args[0]);
    swi_powerdown_us = mp_obj_get_int(args[1]);
    swi_powerup_us = mp_obj_get_int(args[2]);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_set_power_timing_obj, 3, 3, swi_set_power_timing);

static mp_obj_t swi_stop(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_bus_stop(mp_hal_get_pin_obj(args[0]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_stop_obj, 1, 2, swi_stop);

static mp_obj_t swi_wakeup(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_bus_wakeup(mp_hal_get_pin_obj(args[0]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_wakeup_obj, 1, 2, swi_wakeup);

static mp_obj_t swi_reset_delay(void) {
    swi_bus_reset_delay();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(swi_reset_delay_obj, swi_reset_delay);

static mp_obj_t swi_powerdown(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_bus_powerdown(mp_hal_get_pin_obj(args[0]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_powerdown_obj, 1, 2, swi_powerdown);

static mp_obj_t swi_powerup(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_bus_powerup(mp_hal_get_pin_obj(args[0]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_powerup_obj, 1, 2, swi_powerup);

static mp_obj_t swi_powercycle(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_bus_powerdown_hardrst(mp_hal_get_pin_obj(args[0]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_powercycle_obj, 1, 2, swi_powercycle);

static mp_obj_t swi_send_raw_word(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    uint32_t raw = (uint32_t)mp_obj_get_int(args[1]);
    bool wait_irq = false;
    bool immediate_irq = false;
    bool strong_pullup = false;
    if (n_args > 2) {
        wait_irq = mp_obj_is_true(args[2]);
    }
    if (n_args > 3) {
        immediate_irq = mp_obj_is_true(args[3]);
    }
    if (n_args > 4) {
        strong_pullup = mp_obj_is_true(args[4]);
    }
    swi_bus_send_raw_word(pin, raw, wait_irq, immediate_irq, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_raw_word_obj, 2, 5, swi_send_raw_word);

static mp_obj_t swi_recv_raw_word(mp_obj_t pin_in) {
    return MP_OBJ_NEW_SMALL_INT((mp_int_t)swi_bus_recv_raw_word(mp_hal_get_pin_obj(pin_in)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(swi_recv_raw_word_obj, swi_recv_raw_word);

static mp_obj_t swi_compute_parity_obj(mp_obj_t cmd_in) {
    return MP_OBJ_NEW_SMALL_INT(swi_compute_parity((uint16_t)mp_obj_get_int(cmd_in)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(swi_compute_parity_fun_obj, swi_compute_parity_obj);

static mp_obj_t swi_add_invert_flag_obj(mp_obj_t cmd_in) {
    return MP_OBJ_NEW_SMALL_INT((mp_int_t)swi_add_invert_flag((uint16_t)mp_obj_get_int(cmd_in)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(swi_add_invert_flag_fun_obj, swi_add_invert_flag_obj);

static mp_obj_t swi_crc16_obj(mp_obj_t data) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    uint16_t crc = 0xffff;
    const uint8_t *buf = (const uint8_t *)bufinfo.buf;
    for (size_t i = 0; i < bufinfo.len; ++i) {
        crc = swi_crc16_update(crc, buf[i]);
    }
    crc = swi_crc16_finish(crc);
    return MP_OBJ_NEW_SMALL_INT(crc);
}
static MP_DEFINE_CONST_FUN_OBJ_1(swi_crc16_fun_obj, swi_crc16_obj);

static mp_obj_t swi_send_raw_word_no_irq(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    uint8_t code = (uint8_t)mp_obj_get_int(args[1]);
    uint8_t data = (uint8_t)mp_obj_get_int(args[2]);
    bool strong_pullup = false;
    if (n_args > 3) {
        strong_pullup = mp_obj_is_true(args[3]);
    }
    swi_send_cmd_with_crc(pin, code, data, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_raw_word_no_irq_obj, 3, 4, swi_send_raw_word_no_irq);

static mp_obj_t swi_send_wd(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(args[1], &bufinfo, MP_BUFFER_READ);
    const uint8_t *buf = (const uint8_t *)bufinfo.buf;
    bool strong_pullup = false;
    if (n_args > 2) {
        strong_pullup = mp_obj_is_true(args[2]);
    }

    uint16_t crc = 0xffff;
    for (size_t i = 0; i < bufinfo.len; ++i) {
        uint32_t cmd = ((uint32_t)SWI_WD << 8) | buf[i];
        cmd = swi_add_invert_flag(swi_compute_parity((uint16_t)cmd));
        swi_bus_send_raw_word(pin, cmd, false, false, strong_pullup);
        crc = swi_crc16_update(crc, cmd & 0xff);
        crc = swi_crc16_update(crc, (cmd >> 8) & 0xff);
        crc = swi_crc16_update(crc, (cmd >> 16) & 0x01);
    }
    crc = swi_crc16_finish(crc);
    swi_send_crc(pin, crc, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_wd_obj, 2, 3, swi_send_wd);

static mp_obj_t swi_send_era(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    uint8_t addr = (uint8_t)mp_obj_get_int(args[1]);
    bool strong_pullup = false;
    if (n_args > 2) {
        strong_pullup = mp_obj_is_true(args[2]);
    }
    swi_send_cmd_with_crc(pin, SWI_ERA, addr, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_era_obj, 2, 3, swi_send_era);

static mp_obj_t swi_send_rra(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    uint8_t addr = (uint8_t)mp_obj_get_int(args[1]);
    size_t len = (size_t)mp_obj_get_int(args[2]);
    bool strong_pullup = false;
    if (n_args > 3) {
        strong_pullup = mp_obj_is_true(args[3]);
    }

    swi_send_cmd_with_crc(pin, SWI_RRA, addr, strong_pullup);

    // Ensure bus released before receive (SDK: immediate receive after STOP)
    // Force release before receive
    swi_bus_release(pin);

    vstr_t vstr;
    vstr_init_len(&vstr, len);
    swi_receive_data(pin, (uint8_t *)vstr.buf, len);
    // Ensure bus released after receive
    swi_bus_release(pin);
    mp_hal_delay_us(swi_tau5_us);
    return mp_obj_new_bytes_from_vstr(&vstr);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_rra_obj, 3, 4, swi_send_rra);

static mp_obj_t swi_send_bres(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_send_cmd_with_crc(pin, SWI_BC, SWI_BRES, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_bres_obj, 1, 2, swi_send_bres);

static mp_obj_t swi_send_pdwn(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    swi_send_cmd_with_crc(pin, SWI_BC, SWI_PDWN, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_pdwn_obj, 1, 2, swi_send_pdwn);

static mp_obj_t swi_send_rbl(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    int n = mp_obj_get_int(args[1]);
    bool strong_pullup = false;
    if (n_args > 2) {
        strong_pullup = mp_obj_is_true(args[2]);
    }
    uint8_t code;
    switch (n & 0x03) {
        case 0: code = SWI_RBL0; break;
        case 1: code = SWI_RBL1; break;
        case 2: code = SWI_RBL2; break;
        default: code = SWI_RBL3; break;
    }
    swi_send_cmd_with_crc(pin, SWI_BC, code, strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(swi_send_rbl_obj, 2, 3, swi_send_rbl);

static const mp_rom_map_elem_t swi_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR__swi) },

    { MP_ROM_QSTR(MP_QSTR_set_timing), MP_ROM_PTR(&swi_set_timing_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_wakeup_timing), MP_ROM_PTR(&swi_set_wakeup_timing_obj) },
    { MP_ROM_QSTR(MP_QSTR_set_power_timing), MP_ROM_PTR(&swi_set_power_timing_obj) },
    { MP_ROM_QSTR(MP_QSTR_stop), MP_ROM_PTR(&swi_stop_obj) },
    { MP_ROM_QSTR(MP_QSTR_wakeup), MP_ROM_PTR(&swi_wakeup_obj) },
    { MP_ROM_QSTR(MP_QSTR_reset_delay), MP_ROM_PTR(&swi_reset_delay_obj) },
    { MP_ROM_QSTR(MP_QSTR_powerdown), MP_ROM_PTR(&swi_powerdown_obj) },
    { MP_ROM_QSTR(MP_QSTR_powerup), MP_ROM_PTR(&swi_powerup_obj) },
    { MP_ROM_QSTR(MP_QSTR_powercycle), MP_ROM_PTR(&swi_powercycle_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_raw_word), MP_ROM_PTR(&swi_send_raw_word_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv_raw_word), MP_ROM_PTR(&swi_recv_raw_word_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_raw_word_no_irq), MP_ROM_PTR(&swi_send_raw_word_no_irq_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_wd), MP_ROM_PTR(&swi_send_wd_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_era), MP_ROM_PTR(&swi_send_era_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_rra), MP_ROM_PTR(&swi_send_rra_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_bres), MP_ROM_PTR(&swi_send_bres_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_pdwn), MP_ROM_PTR(&swi_send_pdwn_obj) },
    { MP_ROM_QSTR(MP_QSTR_send_rbl), MP_ROM_PTR(&swi_send_rbl_obj) },
    { MP_ROM_QSTR(MP_QSTR_compute_parity), MP_ROM_PTR(&swi_compute_parity_fun_obj) },
    { MP_ROM_QSTR(MP_QSTR_add_invert_flag), MP_ROM_PTR(&swi_add_invert_flag_fun_obj) },
    { MP_ROM_QSTR(MP_QSTR_crc16), MP_ROM_PTR(&swi_crc16_fun_obj) },
};

static MP_DEFINE_CONST_DICT(swi_module_globals, swi_module_globals_table);

const mp_obj_module_t mp_module_swi = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&swi_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__swi, mp_module_swi);

#endif // MICROPY_PY_SWI
