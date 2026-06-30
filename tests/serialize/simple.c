#include "simple.parser.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>


static const char *data = "{\"name\":\"widget\",\"count\":42,\"ok\":false}";

int main(int argc, char** argv){
    (void)argc;
    (void)argv;

    root_t root = {};
    assert(!json_parse_root(data, &root));
    assert(!strcmp(root.name, "widget"));
    assert(root.count == 42);
    assert(root.ok == false);

    char buf[128];
    json_write_state_t state = {
        .buf = buf,
        .capacity = sizeof(buf),
        .pos = 0,
    };
    assert(!json_write_root(&state, &root));
    assert(!strcmp(buf, "{\"name\":\"widget\",\"count\":42,\"ok\":false}"));

    root_t roundtrip = {};
    assert(!json_parse_root(buf, &roundtrip));
    assert(!strcmp(roundtrip.name, root.name));
    assert(roundtrip.count == root.count);
    assert(roundtrip.ok == root.ok);

    state.pos = 0;
    buf[0] = 'x';
    state.capacity = 8;
    assert(json_write_root(&state, &root));

    return 0;
}