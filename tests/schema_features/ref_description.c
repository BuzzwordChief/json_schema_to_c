#include "ref_description.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


const char* data = "{\"timestamp\": \"1710000000000000000\"}";

int main(int argc, char** argv){
    (void)argc;
    (void)argv;
    root_t root = {};
    assert(!json_parse_root(data, &root));
    assert(!strcmp(root.timestamp, "1710000000000000000"));
    return 0;
}