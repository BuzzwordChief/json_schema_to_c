#include "custom_double.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


static int custom_str_to_double(const char *str, double *out) {
    if (str[0] == 'x') {
        return 0;
    }
    return sscanf(str, "%lf", out) == 1;
}

static void custom_double_to_str(double value, char result[JS2C_DOUBLE_STR_MAX_LEN]) {
    snprintf(result, JS2C_DOUBLE_STR_MAX_LEN, "CUSTOM:%.1f", value);
}


int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    js2c_set_str_to_double(custom_str_to_double);
    js2c_set_double_to_str(custom_double_to_str);

    double value = 0;
    assert(!json_parse_root("42.5 ", &value));
    assert(value == 42.5);
    assert(json_parse_root("x1 ", &value));

    char buf[32];
    json_write_state_t state = {
        .buf = buf,
        .capacity = sizeof(buf),
        .pos = 0,
    };
    value = 3.25;
    assert(!json_write_root(&state, &value));
    assert(!strcmp(buf, "CUSTOM:3.2"));

    return 0;
}