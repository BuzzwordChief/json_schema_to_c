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

WORST_CASE_BOOL = 5
WORST_CASE_INT64 = 21
WORST_CASE_QUOTED_INT64 = 23
WORST_CASE_DOUBLE = 24
WORST_CASE_NULL = 4


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


def quoted_enum_literal_len(enum_label):
    return len('"' + json_string_for_literal(enum_label) + '"')


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


def len_var_name(in_expr):
    name = (
        in_expr
        .replace("->", "_")
        .replace(".", "_")
        .replace("[", "_")
        .replace("]", "")
        .replace("*", "")
    )
    if name.startswith("in_"):
        name = name[3:]
    return "js2c_len_{}".format(name)


def emit_lit(out_file, literal, fast=False):
    escaped = c_string_literal(literal)
    if fast:
        out_file.print('json_write_lit_fast(state, "{}");'.format(escaped))
    else:
        out_file.print('err |= json_write_lit(state, "{}");'.format(escaped))


def emit_err(out_file, expression):
    out_file.print("err |= {};".format(expression))


def emit_key_prefix(out_file, field_name, is_first, fast=False):
    emit_lit(out_file, object_key_prefix(field_name, is_first), fast=fast)


def _in_loop_context(in_expr):
    return "[" in in_expr


def emit_add_escaped_cstr_len(out_file, in_expr):
    len_var = "js2c_tmp_len" if _in_loop_context(in_expr) else len_var_name(in_expr)
    if _in_loop_context(in_expr):
        out_file.print("{")
        out_file.indent_level += 4
    out_file.print("size_t {} = json_escaped_cstr_len({});".format(len_var, in_expr))
    out_file.print("if ({} == SIZE_MAX) {{".format(len_var))
    out_file.indent_level += 4
    out_file.print("need = SIZE_MAX;")
    out_file.indent_level -= 4
    out_file.print("} else {")
    out_file.indent_level += 4
    out_file.print("need += {};".format(len_var))
    out_file.indent_level -= 4
    out_file.print("}")
    if _in_loop_context(in_expr):
        out_file.indent_level -= 4
        out_file.print("}")


def emit_escaped_cstr_write(out_file, in_expr, fast=False):
    if fast:
        out_file.print("json_write_escaped_cstr_fast(state, {});".format(in_expr))
        return
    len_var = "js2c_tmp_len" if _in_loop_context(in_expr) else len_var_name(in_expr)
    if _in_loop_context(in_expr):
        out_file.print("{")
        out_file.indent_level += 4
        out_file.print(
            "size_t {} = json_escaped_cstr_len({});".format(len_var, in_expr)
        )
        emit_err(
            out_file,
            "json_write_escaped_cstr_with_len(state, {}, {})"
            .format(in_expr, len_var)
        )
        out_file.indent_level -= 4
        out_file.print("}")
    else:
        emit_err(
            out_file,
            "json_write_escaped_cstr_with_len(state, {}, {})"
            .format(in_expr, len_var)
        )


def emit_writer_epilogue(out_file):
    out_file.print("if (state->capacity) state->buf[state->pos] = '\\0';")
    out_file.print("return false;")


def generate_dual_path_writer(out_file, writer_name, c_type, emit_preamble, emit_body):
    out_file.print(
        "bool {}(json_write_state_t *state, const {} *in)"
        .format(writer_name, c_type)
    )
    out_file.print("{")
    out_file.indent_level += 4
    emit_preamble(out_file)
    with out_file.if_block("state->pos + need >= state->capacity"):
        out_file.print("bool err = false;")
        emit_body(out_file, fast=False)
        out_file.print("if (err) return true;")
    out_file.print("else")
    with out_file.code_block():
        emit_body(out_file, fast=True)
    emit_writer_epilogue(out_file)
    out_file.indent_level -= 4
    out_file.print("}")
    out_file.print("")
