/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2015-2017 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdint.h>

#include "py/obj.h"
#include "py/mphal.h"

#if MICROPY_PY_ONEWIRE

/******************************************************************************/
// Low-level 1-Wire routines

#define TIMING_RESET1 (480)
#define TIMING_RESET2 (70)
#define TIMING_RESET3 (410)
#define TIMING_READ1  (6)
#define TIMING_READ2  (9)
#define TIMING_READ3  (55)
#define TIMING_WRITE1 (6)
#define TIMING_WRITE2 (54)
#define TIMING_WRITE3 (10)

static inline void onewire_pin_input_od(mp_hal_pin_obj_t pin) {
    mp_hal_pin_od_high(pin);
}

static inline void onewire_pin_output_value(mp_hal_pin_obj_t pin, int value, bool strong_pullup) {
    if (strong_pullup) {
        mp_hal_pin_output(pin);
        if (value) {
            mp_hal_pin_high(pin);
        } else {
            mp_hal_pin_low(pin);
        }
    } else {
        if (value) {
            mp_hal_pin_od_high(pin);
        } else {
            mp_hal_pin_od_low(pin);
        }
    }
}

static int onewire_bus_reset(mp_hal_pin_obj_t pin, bool strong_pullup) {
    onewire_pin_output_value(pin, 0, strong_pullup);
    mp_hal_delay_us(TIMING_RESET1);
    uint32_t i = mp_hal_quiet_timing_enter();
    onewire_pin_input_od(pin);
    mp_hal_delay_us_fast(TIMING_RESET2);
    int status = !mp_hal_pin_read(pin);
    mp_hal_quiet_timing_exit(i);
    mp_hal_delay_us(TIMING_RESET3);
    return status;
}

static int onewire_bus_readbit(mp_hal_pin_obj_t pin) {
    onewire_pin_input_od(pin);
    uint32_t i = mp_hal_quiet_timing_enter();
    mp_hal_pin_od_low(pin);
    mp_hal_delay_us_fast(TIMING_READ1);
    onewire_pin_input_od(pin);
    mp_hal_delay_us_fast(TIMING_READ2);
    int value = mp_hal_pin_read(pin);
    mp_hal_quiet_timing_exit(i);
    mp_hal_delay_us_fast(TIMING_READ3);
    return value;
}

static void onewire_bus_writebit(mp_hal_pin_obj_t pin, int value, bool strong_pullup) {
    uint32_t i = mp_hal_quiet_timing_enter();
    onewire_pin_output_value(pin, 0, strong_pullup);
    mp_hal_delay_us_fast(TIMING_WRITE1);
    if (value) {
        onewire_pin_output_value(pin, 1, strong_pullup);
    }
    mp_hal_delay_us_fast(TIMING_WRITE2);
    onewire_pin_input_od(pin);
    mp_hal_delay_us_fast(TIMING_WRITE3);
    mp_hal_quiet_timing_exit(i);
}

/******************************************************************************/
// MicroPython bindings

static mp_obj_t onewire_reset(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 1) {
        strong_pullup = mp_obj_is_true(args[1]);
    }
    return mp_obj_new_bool(onewire_bus_reset(mp_hal_get_pin_obj(args[0]), strong_pullup));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(onewire_reset_obj, 1, 2, onewire_reset);

static mp_obj_t onewire_readbit(mp_obj_t pin_in) {
    return MP_OBJ_NEW_SMALL_INT(onewire_bus_readbit(mp_hal_get_pin_obj(pin_in)));
}
static MP_DEFINE_CONST_FUN_OBJ_1(onewire_readbit_obj, onewire_readbit);

static mp_obj_t onewire_readbyte(mp_obj_t pin_in) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(pin_in);
    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value |= onewire_bus_readbit(pin) << i;
    }
    return MP_OBJ_NEW_SMALL_INT(value);
}
static MP_DEFINE_CONST_FUN_OBJ_1(onewire_readbyte_obj, onewire_readbyte);

static mp_obj_t onewire_writebit(size_t n_args, const mp_obj_t *args) {
    bool strong_pullup = false;
    if (n_args > 2) {
        strong_pullup = mp_obj_is_true(args[2]);
    }
    onewire_bus_writebit(mp_hal_get_pin_obj(args[0]), mp_obj_get_int(args[1]), strong_pullup);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(onewire_writebit_obj, 2, 3, onewire_writebit);

static mp_obj_t onewire_writebyte(size_t n_args, const mp_obj_t *args) {
    mp_hal_pin_obj_t pin = mp_hal_get_pin_obj(args[0]);
    int value = mp_obj_get_int(args[1]);
    bool strong_pullup = false;
    if (n_args > 2) {
        strong_pullup = mp_obj_is_true(args[2]);
    }
    for (int i = 0; i < 8; ++i) {
        onewire_bus_writebit(pin, value & 1, strong_pullup);
        value >>= 1;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(onewire_writebyte_obj, 2, 3, onewire_writebyte);

static mp_obj_t onewire_crc8(mp_obj_t data) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(data, &bufinfo, MP_BUFFER_READ);
    uint8_t crc = 0;
    for (size_t i = 0; i < bufinfo.len; ++i) {
        uint8_t byte = ((uint8_t *)bufinfo.buf)[i];
        for (int b = 0; b < 8; ++b) {
            uint8_t fb_bit = (crc ^ byte) & 0x01;
            if (fb_bit == 0x01) {
                crc = crc ^ 0x18;
            }
            crc = (crc >> 1) & 0x7f;
            if (fb_bit == 0x01) {
                crc = crc | 0x80;
            }
            byte = byte >> 1;
        }
    }
    return MP_OBJ_NEW_SMALL_INT(crc);
}
static MP_DEFINE_CONST_FUN_OBJ_1(onewire_crc8_obj, onewire_crc8);

static const mp_rom_map_elem_t onewire_module_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_onewire) },

    { MP_ROM_QSTR(MP_QSTR_reset), MP_ROM_PTR(&onewire_reset_obj) },
    { MP_ROM_QSTR(MP_QSTR_readbit), MP_ROM_PTR(&onewire_readbit_obj) },
    { MP_ROM_QSTR(MP_QSTR_readbyte), MP_ROM_PTR(&onewire_readbyte_obj) },
    { MP_ROM_QSTR(MP_QSTR_writebit), MP_ROM_PTR(&onewire_writebit_obj) },
    { MP_ROM_QSTR(MP_QSTR_writebyte), MP_ROM_PTR(&onewire_writebyte_obj) },
    { MP_ROM_QSTR(MP_QSTR_crc8), MP_ROM_PTR(&onewire_crc8_obj) },
};

static MP_DEFINE_CONST_DICT(onewire_module_globals, onewire_module_globals_table);

const mp_obj_module_t mp_module_onewire = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&onewire_module_globals,
};

MP_REGISTER_MODULE(MP_QSTR__onewire, mp_module_onewire);

#endif // MICROPY_PY_ONEWIRE
