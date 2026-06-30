#include "one_of.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


int main(int argc, char** argv){
    (void)argc;
    (void)argv;

    char buf[128];
    json_write_state_t state = {
        .buf = buf,
        .capacity = sizeof(buf),
        .pos = 0,
    };

    root_alpha_t alpha = {
        .type = ROOT_ALPHA_TYPE_ALPHA,
        .value = 99,
    };
    assert(!json_write_root_alpha(&state, &alpha));
    assert(!strcmp(buf, "{\"type\":\"alpha\",\"value\":99}"));

    root_t parsed = {};
    assert(!json_parse_root(buf, &parsed));
    assert(parsed.variant == ROOT_ALPHA);
    assert(parsed.u.alpha.value == 99);

    state.pos = 0;
    root_beta_t beta = {
        .type = ROOT_BETA_TYPE_BETA,
    };
    memcpy(beta.label, "hello", 6);
    assert(!json_write_root_beta(&state, &beta));
    assert(!strcmp(buf, "{\"type\":\"beta\",\"label\":\"hello\"}"));

    assert(!json_parse_root(buf, &parsed));
    assert(parsed.variant == ROOT_BETA);
    assert(!strcmp(parsed.u.beta.label, "hello"));

    return 0;
}