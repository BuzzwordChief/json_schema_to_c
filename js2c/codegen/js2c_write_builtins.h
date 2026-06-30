/*
 * MIT License
 *
 * Copyright (c) 2020 Alex Badics
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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef JS2C_WRITE_BUILTINS_H
#define JS2C_WRITE_BUILTINS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define JS2C_DOUBLE_STR_MAX_LEN 25

#ifndef JS2C_DOUBLE_HOOKS_DECLARED
#define JS2C_DOUBLE_HOOKS_DECLARED
typedef int (*js2c_str_to_double_fn_t)(const char *str, double *out);
typedef void (*js2c_double_to_str_fn_t)(double value, char result[JS2C_DOUBLE_STR_MAX_LEN]);
#endif

void js2c_set_str_to_double(js2c_str_to_double_fn_t fn);
void js2c_set_double_to_str(js2c_double_to_str_fn_t fn);
void js2c_format_double(double value, char result[JS2C_DOUBLE_STR_MAX_LEN]);

typedef struct json_write_state_s {
    char *buf;
    size_t capacity;
    size_t pos;
} json_write_state_t;

/* true = error (typically buffer overflow), false = success */
static inline bool json_write_bytes(json_write_state_t *state, const char *data, size_t len) {
    if (state->pos + len > state->capacity) {
        return true;
    }
    for (size_t i = 0; i < len; ++i) {
        state->buf[state->pos + i] = data[i];
    }
    state->pos += len;
    return false;
}

static inline bool json_write_char(json_write_state_t *state, char c) {
    if (state->pos >= state->capacity) {
        return true;
    }
    state->buf[state->pos++] = c;
    return false;
}

static inline bool json_write_cstr(json_write_state_t *state, const char *s) {
    while (*s) {
        if (json_write_char(state, *s++)) {
            return true;
        }
    }
    return false;
}

static inline bool json_write_null(json_write_state_t *state) {
    return json_write_cstr(state, "null");
}

static inline bool json_write_bool(json_write_state_t *state, bool value) {
    return json_write_cstr(state, value ? "true" : "false");
}

static inline bool json_write_uint64_dec(json_write_state_t *state, uint64_t value) {
    char tmp[21];
    int i = 20;
    tmp[i] = '\0';
    if (value == 0) {
        return json_write_char(state, '0');
    }
    while (value > 0) {
        tmp[--i] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    return json_write_cstr(state, tmp + i);
}

static inline bool json_write_int64_dec(json_write_state_t *state, int64_t value) {
    if (value < 0) {
        if (json_write_char(state, '-')) {
            return true;
        }
        return json_write_uint64_dec(state, (uint64_t)(-(value + 1)) + 1U);
    }
    return json_write_uint64_dec(state, (uint64_t)value);
}

static inline bool json_write_quoted_uint64_dec(json_write_state_t *state, uint64_t value) {
    if (json_write_char(state, '"')) {
        return true;
    }
    if (json_write_uint64_dec(state, value)) {
        return true;
    }
    return json_write_char(state, '"');
}

static inline bool json_write_escaped_cstr(json_write_state_t *state, const char *s) {
    if (json_write_char(state, '"')) {
        return true;
    }
    for (const char *p = s; *p; ++p) {
        const char c = *p;
        if (c == '"' || c == '\\') {
            if (json_write_char(state, '\\')) {
                return true;
            }
            if (json_write_char(state, c)) {
                return true;
            }
            continue;
        }
        if (c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            if (json_write_char(state, '\\')) {
                return true;
            }
            if (c == '\b' && json_write_char(state, 'b')) {
                return true;
            } else if (c == '\f' && json_write_char(state, 'f')) {
                return true;
            } else if (c == '\n' && json_write_char(state, 'n')) {
                return true;
            } else if (c == '\r' && json_write_char(state, 'r')) {
                return true;
            } else if (c == '\t' && json_write_char(state, 't')) {
                return true;
            }
            continue;
        }
        if ((unsigned char)c < 0x20) {
            return true;
        }
        if (json_write_char(state, c)) {
            return true;
        }
    }
    return json_write_char(state, '"');
}

#endif /* JS2C_WRITE_BUILTINS_H */