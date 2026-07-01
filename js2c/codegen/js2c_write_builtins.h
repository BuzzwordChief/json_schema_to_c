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
#include <string.h>

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

static inline void json_write_bytes_fast(json_write_state_t *state, const char *data, size_t len) {
    memcpy(state->buf + state->pos, data, len);
    state->pos += len;
}

#define json_write_lit_fast(state, lit) \
    json_write_bytes_fast((state), (lit), sizeof(lit) - 1)

/* true = error (typically buffer overflow), false = success */
static inline bool json_write_bytes(json_write_state_t *state, const char *data, size_t len) {
    if (state->pos + len >= state->capacity) {
        return true;
    }
    json_write_bytes_fast(state, data, len);
    return false;
}

#define json_write_lit(state, lit) \
    json_write_bytes((state), (lit), sizeof(lit) - 1)

static inline void json_write_char_fast(json_write_state_t *state, char c) {
    state->buf[state->pos++] = c;
}

static inline bool json_write_char(json_write_state_t *state, char c) {
    if (state->pos + 1 >= state->capacity) {
        return true;
    }
    json_write_char_fast(state, c);
    return false;
}

static inline void json_write_cstr_fast(json_write_state_t *state, const char *s) {
    while (*s) {
        json_write_char_fast(state, *s++);
    }
}

static inline bool json_write_cstr(json_write_state_t *state, const char *s) {
    while (*s) {
        if (json_write_char(state, *s++)) {
            return true;
        }
    }
    return false;
}

static inline size_t json_escaped_cstr_len(const char *s) {
    size_t len = 2;
    for (const char *p = s; *p; ++p) {
        const unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\' || c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            len += 2;
        } else if (c < 0x20) {
            return SIZE_MAX;
        } else {
            len += 1;
        }
    }
    return len;
}

static inline void json_write_escaped_cstr_body_fast(json_write_state_t *state, const char *s) {
    json_write_char_fast(state, '"');
    for (const char *p = s; *p; ++p) {
        const char c = *p;
        if (c == '"' || c == '\\') {
            json_write_char_fast(state, '\\');
            json_write_char_fast(state, c);
            continue;
        }
        if (c == '\b' || c == '\f' || c == '\n' || c == '\r' || c == '\t') {
            json_write_char_fast(state, '\\');
            if (c == '\b') {
                json_write_char_fast(state, 'b');
            } else if (c == '\f') {
                json_write_char_fast(state, 'f');
            } else if (c == '\n') {
                json_write_char_fast(state, 'n');
            } else if (c == '\r') {
                json_write_char_fast(state, 'r');
            } else if (c == '\t') {
                json_write_char_fast(state, 't');
            }
            continue;
        }
        json_write_char_fast(state, c);
    }
    json_write_char_fast(state, '"');
}

static inline void json_write_escaped_cstr_fast(json_write_state_t *state, const char *s) {
    json_write_escaped_cstr_body_fast(state, s);
}

static inline bool json_write_escaped_cstr_with_len(json_write_state_t *state, const char *s, size_t len) {
    if (state->pos + len >= state->capacity) {
        return true;
    }
    json_write_escaped_cstr_body_fast(state, s);
    return false;
}

static inline bool json_write_null(json_write_state_t *state) {
    return json_write_lit(state, "null");
}

static inline void json_write_null_fast(json_write_state_t *state) {
    json_write_lit_fast(state, "null");
}

static inline bool json_write_bool(json_write_state_t *state, bool value) {
    if (value) {
        return json_write_lit(state, "true");
    }
    return json_write_lit(state, "false");
}

static inline void json_write_bool_fast(json_write_state_t *state, bool value) {
    if (value) {
        json_write_lit_fast(state, "true");
    } else {
        json_write_lit_fast(state, "false");
    }
}

static inline void json_write_uint64_dec_fast(json_write_state_t *state, uint64_t value) {
    if (value == 0) {
        json_write_char_fast(state, '0');
        return;
    }
    const size_t start = state->pos;
    size_t end = start;
    while (value > 0) {
        state->buf[end++] = (char)('0' + (value % 10U));
        value /= 10U;
    }
    for (size_t i = 0; i < (end - start) / 2; ++i) {
        const char tmp = state->buf[start + i];
        state->buf[start + i] = state->buf[end - 1 - i];
        state->buf[end - 1 - i] = tmp;
    }
    state->pos = end;
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

static inline void json_write_int64_dec_fast(json_write_state_t *state, int64_t value) {
    if (value < 0) {
        json_write_char_fast(state, '-');
        json_write_uint64_dec_fast(state, (uint64_t)(-(value + 1)) + 1U);
        return;
    }
    json_write_uint64_dec_fast(state, (uint64_t)value);
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

static inline void json_write_quoted_uint64_dec_fast(json_write_state_t *state, uint64_t value) {
    json_write_char_fast(state, '"');
    json_write_uint64_dec_fast(state, value);
    json_write_char_fast(state, '"');
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

static inline void json_write_quoted_int64_dec_fast(json_write_state_t *state, int64_t value) {
    json_write_char_fast(state, '"');
    json_write_int64_dec_fast(state, value);
    json_write_char_fast(state, '"');
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

static inline bool json_write_inline_cstr(json_write_state_t *state, const char *s) {
    return json_write_cstr(state, s);
}

static inline bool json_write_inline_escaped_cstr(json_write_state_t *state, const char *s) {
    return json_write_escaped_cstr(state, s);
}

static inline bool json_write_inline_uint64_dec(json_write_state_t *state, uint64_t value) {
    return json_write_uint64_dec(state, value);
}

static inline bool json_write_inline_int64_dec(json_write_state_t *state, int64_t value) {
    return json_write_int64_dec(state, value);
}

static inline bool json_write_inline_quoted_uint64_dec(json_write_state_t *state, uint64_t value) {
    return json_write_quoted_uint64_dec(state, value);
}

static inline bool json_write_inline_quoted_int64_dec(json_write_state_t *state, int64_t value) {
    if (json_write_char(state, '"')) {
        return true;
    }
    if (json_write_int64_dec(state, value)) {
        return true;
    }
    return json_write_char(state, '"');
}

static inline bool json_write_inline_bool(json_write_state_t *state, bool value) {
    return json_write_bool(state, value);
}

static inline void json_write_double_fast(json_write_state_t *state, double value) {
    char tmp[JS2C_DOUBLE_STR_MAX_LEN];
    js2c_format_double(value, tmp);
    json_write_cstr_fast(state, tmp);
}

static inline bool json_write_inline_double(json_write_state_t *state, double value) {
    char tmp[JS2C_DOUBLE_STR_MAX_LEN];
    js2c_format_double(value, tmp);
    return json_write_inline_cstr(state, tmp);
}

#endif /* JS2C_WRITE_BUILTINS_H */