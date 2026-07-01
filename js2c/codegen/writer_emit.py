#!/usr/bin/env python3
#
# MIT License
#
# Copyright (c) 2020 Alex Badics
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#


def c_string_literal(value):
    return (
        value
        .replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\r", "\\r")
        .replace("\t", "\\t")
    )


def json_string_for_literal(value):
    result = []
    for char in value:
        if char == "\\":
            result.append("\\\\")
        elif char == '"':
            result.append('\\"')
        elif char == "\b":
            result.append("\\b")
        elif char == "\f":
            result.append("\\f")
        elif char == "\n":
            result.append("\\n")
        elif char == "\r":
            result.append("\\r")
        elif char == "\t":
            result.append("\\t")
        elif ord(char) < 0x20:
            result.append("\\u{:04x}".format(ord(char)))
        else:
            result.append(char)
    return "".join(result)


def object_field_path(base_path, field_name):
    if base_path == "in":
        return "in->{}".format(field_name)
    return "{}.{}".format(base_path, field_name)


def array_length_path(base_path):
    if base_path == "in":
        return "in->n"
    return "{}.n".format(base_path)


def array_item_path(base_path, index_expr):
    if base_path == "in":
        return "in->items[{}]".format(index_expr)
    return "{}.items[{}]".format(base_path, index_expr)


def object_key_prefix(field_name, is_first):
    prefix = "{" if is_first else ","
    return prefix + '"' + json_string_for_literal(field_name) + '":'


def emit_lit(out_file, literal):
    out_file.print(
        'err |= json_write_lit(state, "{}");'
        .format(c_string_literal(literal))
    )


def emit_err(out_file, expression):
    out_file.print("err |= {};".format(expression))


def emit_key_prefix(out_file, field_name, is_first):
    emit_lit(out_file, object_key_prefix(field_name, is_first))


def emit_writer_finish(out_file):
    out_file.print("if (err) return true;")
    out_file.print("if (state->capacity) state->buf[state->pos] = '\\0';")
    out_file.print("return false;")


def begin_public_writer(out_file, writer_name, c_type):
    out_file.print(
        "bool {}(json_write_state_t *state, const {} *in)"
        .format(writer_name, c_type)
    )
    out_file.print("{")
    out_file.indent_level += 4
    out_file.print("bool err = false;")


def end_public_writer(out_file):
    emit_writer_finish(out_file)
    out_file.indent_level -= 4
    out_file.print("}")
    out_file.print("")
