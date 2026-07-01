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
from collections import OrderedDict

from .base import Generator, CType, SchemaError
from .writer_emit import (
    WORST_CASE_NULL,
    emit_add_escaped_cstr_len,
    emit_escaped_cstr_write,
    emit_err,
)


NESTED_DEFAULT_MAX_STRING_LENGTH = 256


class StringType(CType):
    def __init__(self, type_name, description, max_length):
        super().__init__(type_name, description)
        self.max_length = max_length

    def generate_type_declaration_impl(self, out_file):
        out_file.print_with_docstring(
            "typedef char {}[{}];".format(self.type_name, self.max_length + 1), self.description
        )
        out_file.print("")

    def __eq__(self, other):
        return (
            super().__eq__(other) and
            self.max_length == other.max_length
        )


class StringGenerator(Generator):
    JSON_FIELDS = Generator.JSON_FIELDS + (
        "minLength",
        "maxLength",
        "default",
        "js2cParseFunction",
    )

    minLength = 0
    maxLength = None
    default = None
    js2cParseFunction = None

    def __init__(self, schema, parameters):
        super().__init__(schema, parameters)
        assert 'enum' not in schema, "Enums should be generated with EnumGenerator"

        if self.maxLength is None:
            if parameters.path_in_schema:
                self.maxLength = NESTED_DEFAULT_MAX_STRING_LENGTH
            else:
                raise SchemaError(self, "Strings must have maxLength")

        if self.default is not None and len(self.default) > self.maxLength:
            raise SchemaError(self, "String default value longer than maxLength")

        if self.default is not None and len(self.default) < self.minLength:
            raise SchemaError(self, "String default value shorter than minLength")

        if self.js2cParseFunction is not None:
            self.c_type = CType(self.js2cType, self.description)
        else:
            self.c_type = StringType(self.type_name, self.description, self.maxLength)
        self.c_type = parameters.type_cache.try_get_cached(self.c_type)

    @classmethod
    def can_parse_schema(cls, schema):
        return schema.get('type') == 'string'

    def generate_custom_parser_call(self, src, src_length, out_var_name, out_file):
        out_file.print("const char *error = NULL;")
        parser_call = "{}({}, {}, {}, &error)".format(self.js2cParseFunction, src, src_length, out_var_name)
        with out_file.if_block(parser_call):
            self.generate_logged_error([
                "Error parsing '%s', value=\\\"%.*s\\\": %s",
                "parse_state->current_key",
                src_length,
                src,
                "error ? error : \"error calling {}\"".format(self.js2cParseFunction),
            ], out_file)

    def generate_parser_call(self, out_var_name, out_file):
        if self.js2cParseFunction is not None:
            length_check = \
                "builtin_check_current_string(parse_state, {}, {})" \
                .format(self.minLength, self.maxLength)
            with out_file.if_block(length_check):
                out_file.print("return true;")

            self.generate_custom_parser_call(
                "CURRENT_STRING(parse_state)",
                "CURRENT_STRING_LENGTH(parse_state)",
                out_var_name,
                out_file
            )
            out_file.print("parse_state->current_token += 1;")
        else:
            length_check = \
                "builtin_parse_string(parse_state, {}[0], {}, {})" \
                .format(out_var_name, self.minLength, self.maxLength)
            with out_file.if_block(length_check):
                out_file.print("return true;")

    def has_default_value(self):
        return super().has_default_value() or self.default is not None

    def generate_set_default_value(self, out_var_name, out_file):
        assert self.has_default_value(), "Caller is responsible for checking this."
        if self.js2cDefault is not None:
            out_file.print(
                'strncpy({dst}, {src}, {size});'.format(
                    dst=out_var_name,
                    src=self.js2cDefault,
                    size=self.maxLength + 1,
                )
            )
        elif self.js2cParseFunction is not None:
            # The custom parser call has to be in its own code block, because
            # it declares a variable, and there are places where this is generated
            # multiple times into the same scope.
            with out_file.code_block(standalone=True):
                self.generate_custom_parser_call(
                    '"{}"'.format(self.default),
                    str(len(self.default)),
                    "&{}".format(out_var_name),
                    out_file
                )
        else:
            out_file.print(
                'memcpy({dst}, "{src}", {size});'.format(
                    dst=out_var_name,
                    src=self.default,
                    size=len(self.default) + 1
                )
            )

    def max_token_num(self):
        return 1

    def emit_need_computation(self, in_expr, out_file):
        if self.js2cParseFunction is not None:
            out_file.print("need = SIZE_MAX;")
            return
        emit_add_escaped_cstr_len(out_file, in_expr)

    def emit_writer_inline(self, in_expr, out_file, fast=False):
        if self.js2cParseFunction is not None:
            if not fast:
                out_file.print("(void){};".format(in_expr))
                out_file.print("err = true;")
            return
        emit_escaped_cstr_write(out_file, in_expr, fast=fast)

    def generate_writer_bodies(self, out_file):
        self.generate_leaf_writer_bodies(out_file)


class NullableType(CType):
    def __init__(self, type_name, description, inner_type):
        super().__init__(type_name, description)
        self.inner_type = inner_type

    def generate_type_declaration_impl(self, out_file):
        self.inner_type.generate_type_declaration(out_file)
        out_file.print("typedef struct {")
        with out_file.indent():
            out_file.print("bool is_null;")
            out_file.print("{} value;".format(self.inner_type))
        out_file.print("}} {};".format(self.type_name))
        out_file.print("")

    def generate_field_declaration(self, field_name, out_file):
        out_file.print_with_docstring(
            "{} {};".format(self.type_name, field_name), self.description
        )

    def __eq__(self, other):
        return (
            super().__eq__(other) and
            self.inner_type == other.inner_type
        )


class StringOrNullAnyOfGenerator(Generator):
    def __init__(self, schema, parameters):
        non_null_schemas = [item for item in schema['anyOf'] if item.get('type') != 'null']
        if len(non_null_schemas) != 1:
            raise SchemaError(self, "anyOf with null must have exactly one non-null schema")

        merged_schema = OrderedDict(schema)
        merged_schema.update(non_null_schemas[0])
        merged_schema.pop('anyOf', None)
        super().__init__(merged_schema, parameters)

        self.inner_generator = parameters.generator_factory.get_generator_for(
            merged_schema,
            parameters.with_suffix("anyOf.value", self.type_name, "value"),
        )
        self.c_type = NullableType(self.type_name, self.description, self.inner_generator.c_type)
        self.c_type = parameters.type_cache.try_get_cached(self.c_type)

    @classmethod
    def can_parse_schema(cls, schema):
        if "anyOf" not in schema or len(schema['anyOf']) != 2:
            return False
        has_null = False
        non_null_count = 0
        for item in schema['anyOf']:
            if item.get('type') == 'null':
                has_null = True
            else:
                non_null_count += 1
        return has_null and non_null_count == 1

    @classmethod
    def nullable_member(cls, out_var_name, member):
        return "({})->{}".format(out_var_name, member)

    def generate_parser_call(self, out_var_name, out_file):
        with out_file.if_block(
            "!check_type(parse_state, JSMN_PRIMITIVE) && "
            "current_primitive_is(parse_state, \"null\")"
        ):
            out_file.print(
                "{} = true;".format(self.nullable_member(out_var_name, "is_null"))
            )
            out_file.print("parse_state->current_token += 1;")
        out_file.print("else")
        with out_file.code_block():
            out_file.print(
                "{} = false;".format(self.nullable_member(out_var_name, "is_null"))
            )
            self.inner_generator.generate_parser_call(
                "&" + self.nullable_member(out_var_name, "value"),
                out_file
            )

    def generate_parser_bodies(self, out_file):
        self.inner_generator.generate_parser_bodies(out_file)

    def has_default_value(self):
        return self.inner_generator.has_default_value()

    def generate_set_default_value(self, out_var_name, out_file):
        out_file.print(
            "{} = false;".format(self.nullable_member(out_var_name, "is_null"))
        )
        self.inner_generator.generate_set_default_value(
            self.nullable_member(out_var_name, "value"),
            out_file
        )

    def max_token_num(self):
        return self.inner_generator.max_token_num()

    @classmethod
    def nullable_member_path(cls, in_expr, member):
        if in_expr == "in":
            return "in->{}".format(member)
        return "{}.{}".format(in_expr, member)

    def writer_static_worst_case(self):
        return max(WORST_CASE_NULL, self.inner_generator.writer_static_worst_case())

    def emit_need_computation(self, in_expr, out_file):
        # Always precompute inner need (and string length vars) so slow-path writes
        # can reuse them. Overestimates when is_null, which only skips fast path.
        self.inner_generator.emit_need_computation(
            self.nullable_member_path(in_expr, "value"),
            out_file,
        )

    def emit_writer_inline(self, in_expr, out_file, fast=False):
        with out_file.if_block(self.nullable_member_path(in_expr, "is_null")):
            if fast:
                out_file.print("json_write_null_fast(state);")
            else:
                emit_err(out_file, "json_write_null(state)")
        out_file.print("else")
        with out_file.code_block():
            self.inner_generator.emit_writer_inline(
                self.nullable_member_path(in_expr, "value"),
                out_file,
                fast=fast,
            )

    def generate_writer_bodies(self, out_file):
        self.generate_dual_path_writer(out_file)
