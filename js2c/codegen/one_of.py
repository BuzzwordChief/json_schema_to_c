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
import re

from .base import Generator, CType, SchemaError


class OneOfType(CType):
    def __init__(self, type_name, description, variants):
        super().__init__(type_name, description)
        self.variants = variants

    def generate_type_declaration_impl(self, out_file):
        for _, variant_type in self.variants:
            variant_type.generate_type_declaration(out_file)

        enum_name = "{}_variant_t".format(self.type_name)
        out_file.print("typedef enum {}_e ".format(self.type_name) + "{")
        with out_file.indent():
            for variant_label, _ in self.variants[:-1]:
                out_file.print("{},"
                               .format(OneOfGenerator.format_variant_enum_label(self.type_name, variant_label)))
            out_file.print(OneOfGenerator.format_variant_enum_label(self.type_name, self.variants[-1][0]))
        out_file.print("}} {};".format(enum_name))
        out_file.print("")

        out_file.print("typedef struct {}_s ".format(self.type_name) + "{")
        with out_file.indent():
            out_file.print("{} variant;".format(enum_name))
            out_file.print("union {")
            with out_file.indent():
                for variant_label, variant_type in self.variants:
                    out_file.print("{} {};"
                                   .format(variant_type, variant_label))
            out_file.print("} u;")
        out_file.print("}} {};".format(self.type_name))
        out_file.print("")

    def __eq__(self, other):
        return (
            super().__eq__(other) and
            self.variants == other.variants
        )


class OneOfGenerator(Generator):
    SANITIZE_RE = re.compile("[^A-Za-z0-9_]")

    def __init__(self, schema, parameters):
        super().__init__(schema, parameters)
        if 'oneOf' not in schema or not schema['oneOf']:
            raise SchemaError(self, "oneOf must be a non-empty array")

        self.variants = []
        used_labels = set()
        for i, variant_schema in enumerate(schema['oneOf']):
            variant_label = self.variant_label_from_schema(variant_schema, i)
            if variant_label in used_labels:
                raise SchemaError(self, "Duplicate oneOf variant label '{}'".format(variant_label))
            used_labels.add(variant_label)

            variant_generator = parameters.generator_factory.get_generator_for(
                variant_schema,
                parameters.with_suffix("oneOf[{}]".format(i), self.type_name, variant_label),
            )
            self.variants.append((variant_label, variant_generator))

        self.c_type = OneOfType(
            self.type_name,
            self.description,
            tuple((label, gen.c_type) for label, gen in self.variants)
        )
        self.c_type = parameters.type_cache.try_get_cached(self.c_type)

    @classmethod
    def can_parse_schema(cls, schema):
        return isinstance(schema.get('oneOf'), list) and len(schema['oneOf']) > 0

    @classmethod
    def format_variant_enum_label(cls, type_name, variant_label):
        prefix = re.sub("_t$", "", type_name).upper()
        return "{}_{}".format(prefix, variant_label.upper())

    @classmethod
    def variant_label_from_schema(cls, schema, index):
        type_schema = schema.get('properties', {}).get('type', {})
        if 'enum' in type_schema and len(type_schema['enum']) == 1:
            return cls.sanitize_variant_label(type_schema['enum'][0])
        if 'const' in type_schema:
            return cls.sanitize_variant_label(type_schema['const'])
        if '$id' in schema:
            return cls.sanitize_variant_label(schema['$id'])
        return "v{}".format(index)

    @classmethod
    def sanitize_variant_label(cls, label):
        label = cls.SANITIZE_RE.sub("_", str(label))
        if label and label[0].isdigit():
            label = "_{}".format(label)
        return label

    def generate_parser_call(self, out_var_name, out_file):
        parser_call = "parse_{}(parse_state, {})".format(self.parser_name, out_var_name)
        with out_file.if_block(parser_call):
            out_file.print("return true;")

    def generate_parser_bodies(self, out_file):
        for _, variant_generator in self.variants:
            variant_generator.generate_parser_bodies(out_file)

        out_file.print(
            "static bool parse_{}(parse_state_t *parse_state, {} *out)"
            .format(self.parser_name, self.c_type)
        )
        with out_file.code_block():
            with out_file.if_block("check_type(parse_state, JSMN_OBJECT)"):
                out_file.print("return true;")

            out_file.print("const int saved_current_token = parse_state->current_token;")
            for variant_label, variant_generator in self.variants:
                enum_label = self.format_variant_enum_label(self.type_name, variant_label)
                out_file.print("parse_state->current_token = saved_current_token;")
                with out_file.if_block(
                    "!parse_{}(parse_state, &out->u.{})"
                    .format(variant_generator.parser_name, variant_label)
                ):
                    out_file.print("out->variant = {};".format(enum_label))
                    out_file.print("return false;")

            self.generate_logged_error("No matching oneOf variant for '%s'", out_file)
            out_file.print("parse_state->current_token = saved_current_token;")
        out_file.print("")

    def max_token_num(self):
        return max(variant_generator.max_token_num() for _, variant_generator in self.variants)