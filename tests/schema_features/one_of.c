#include "one_of.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


const char* alpha_data = "{\"type\": \"alpha\", \"value\": 42}";
const char* beta_data = "{\"type\": \"beta\", \"label\": \"hello\"}";

int main(int argc, char** argv){
    (void)argc;
    (void)argv;
    root_t root = {};

    assert(!json_parse_root(alpha_data, &root));
    assert(root.variant == ROOT_ALPHA);
    assert(root.u.alpha.value == 42);

    assert(!json_parse_root(beta_data, &root));
    assert(root.variant == ROOT_BETA);
    assert(!strcmp(root.u.beta.label, "hello"));

    return 0;
}