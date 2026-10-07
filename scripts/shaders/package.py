#!/usr/bin/env python3
"""Compile a bounded expression manifest to Native's portable image profile.

Expressions are parsed as data with ast; no Python/effect code is executed.
Only the standard library is needed. The runtime validates the emitted package
again before installation. MIT License, Copyright (C) 2026 Tomaz Stih.
"""
import argparse
import ast
import json
import math
from pathlib import Path
import re

IDENTIFIER = re.compile(r"[a-zA-Z_][a-zA-Z_0-9]{0,63}\Z")
UNARY = {"sin", "cos", "exp", "floor", "abs", "sqrt", "clamp"}
BINARY = {"min", "max", "pow", "dot", "step"}
TYPES = {"int": 0, "float": 1, "float2": 2, "float3": 3, "float4": 4}


def number(value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ValueError("Expected a finite number")
    if not math.isfinite(value) or abs(value) > 3.4028234e38:
        raise ValueError("Number does not fit a finite shader float")
    return format(value, ".9g")


class Program:
    def __init__(self, parameters, textures):
        self.registers = {"uv": 0, "source_info": 1, "clock": 2}
        self.registers.update({p["name"]: i + 3 for i, p in enumerate(parameters)})
        self.textures = textures
        self.lines = []
        self.next_register = 20
        self.constants = {}

    def emit(self, operation, *args):
        if self.next_register >= 64:
            raise ValueError("Program exceeds 44 writable vector registers")
        result = self.next_register
        self.next_register += 1
        self.lines.append(f"{operation} {result} " + " ".join(map(str, args)))
        return result

    def constant(self, values):
        if len(values) != 4:
            raise ValueError("vec4 requires four numeric constants")
        key = tuple(number(v) for v in values)
        if key not in self.constants:
            self.constants[key] = self.emit("const", *key)
        return self.constants[key]

    def expression(self, node):
        if isinstance(node, ast.Constant):
            return self.constant([node.value] * 4)
        if isinstance(node, ast.Name) and node.id in self.registers:
            return self.registers[node.id]
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
            return self.emit("sub", self.constant([0] * 4), self.expression(node.operand))
        if isinstance(node, ast.BinOp):
            names = {ast.Add: "add", ast.Sub: "sub", ast.Mult: "mul", ast.Div: "div"}
            if type(node.op) not in names:
                raise ValueError("Unsupported arithmetic operation")
            return self.emit(names[type(node.op)], self.expression(node.left), self.expression(node.right))
        if isinstance(node, ast.Attribute):
            channels = "xyzw" if all(c in "xyzw" for c in node.attr) else "rgba"
            if not node.attr or len(node.attr) > 4 or any(c not in channels for c in node.attr):
                raise ValueError("Invalid vector swizzle")
            indices = [channels.index(c) for c in node.attr]
            indices = (indices * 4)[:4]
            return self.emit("swizzle", self.expression(node.value), *indices)
        if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and not node.keywords:
            name = node.func.id
            if name == "vec4" and len(node.args) == 4:
                values = []
                for arg in node.args:
                    sign = 1
                    if isinstance(arg, ast.UnaryOp) and isinstance(arg.op, ast.USub):
                        sign, arg = -1, arg.operand
                    if not isinstance(arg, ast.Constant):
                        raise ValueError("vec4 accepts numeric constants only")
                    values.append(sign * arg.value)
                return self.constant(values)
            if name in {"sample", "linear"} and len(node.args) == 2:
                texture = node.args[0]
                if not isinstance(texture, ast.Name) or texture.id not in self.textures:
                    raise ValueError("Unknown or forward texture dependency")
                return self.emit(name, self.textures[texture.id], self.expression(node.args[1]))
            if ((name in UNARY and len(node.args) == 1) or
                (name in BINARY and len(node.args) == 2) or
                (name == "mix" and len(node.args) == 3)):
                return self.emit(name, *(self.expression(arg) for arg in node.args))
        raise ValueError("Expression uses an unsupported construct")

    def assign(self, name, expression):
        if not IDENTIFIER.fullmatch(name) or name in self.registers:
            raise ValueError("Invalid or duplicate expression name")
        if not isinstance(expression, str) or len(expression) > 4096:
            raise ValueError("Expression must be bounded text")
        self.registers[name] = self.expression(ast.parse(expression, mode="eval").body)


def compile_package(manifest):
    if manifest.get("profile") != "native-image-1":
        raise ValueError("Unknown image-shader profile")
    parameters = manifest.get("parameters", [])
    passes = manifest.get("passes", [])
    if len(parameters) > 16 or not 1 <= len(passes) <= 8:
        raise ValueError("Profile allows at most 16 parameters and 1..8 passes")
    lines = ["native-image-1 1", f"parameters {len(parameters)}"]
    names = {"uv", "source_info", "clock"}
    for parameter in parameters:
        name = parameter["name"]
        if not IDENTIFIER.fullmatch(name) or name in names:
            raise ValueError("Invalid or reserved parameter name")
        names.add(name)
        kind = TYPES[parameter["type"]]
        lo, hi = parameter["range"]
        number(lo), number(hi)
        value = parameter["default"]
        values = [value] if kind < 2 else value
        if len(values) != max(1, kind) or lo > hi or any(v < lo or v > hi for v in values):
            raise ValueError("Invalid default or parameter range")
        if kind == 0 and (isinstance(value, bool) or not isinstance(value, int) or
                          not -2147483648 <= value <= 2147483647):
            raise ValueError("Integer default must fit int32")
        defaults = str(value) if kind == 0 else " ".join(number(v) for v in values)
        lines.append(f"parameter {name} {kind} {number(lo)} {number(hi)} " + defaults)
    lines.append(f"passes {len(passes)}")
    textures = {"source": -1}
    pass_names = set()
    for index, descriptor in enumerate(passes):
        name = descriptor["name"]
        if not IDENTIFIER.fullmatch(name) or name in pass_names or name == "source" or name.startswith("history_"):
            raise ValueError("Invalid, duplicate or reserved pass name")
        pass_names.add(name)
        if descriptor.get("history", False):
            textures["history_" + name] = -index - 2
    for index, descriptor in enumerate(passes):
        program = Program(parameters, textures)
        for assignment in descriptor["program"]:
            if len(assignment) != 2:
                raise ValueError("Program entries are name/expression pairs")
            program.assign(*assignment)
        output = program.registers[descriptor["output"]]
        extent = descriptor.get("extent", "source")
        reduction = descriptor.get("reduction", 1)
        if extent not in {"source", "viewport"} or not isinstance(reduction, int) or not 1 <= reduction <= 16:
            raise ValueError("Invalid target extent/reduction")
        if not 1 <= len(program.lines) <= 128:
            raise ValueError("Program must contain 1..128 instructions")
        lines.append(f"pass {descriptor['name']} {extent} {reduction} {int(bool(descriptor.get('history', False)))} {output} {len(program.lines)}")
        lines.extend(program.lines)
        textures[descriptor["name"]] = index
    lines.append("end")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        if args.source.stat().st_size > 1024 * 1024:
            raise ValueError("Manifest exceeds one MiB")
        package = compile_package(json.loads(args.source.read_text(encoding="utf-8")))
        args.output.write_text(package, encoding="ascii")
    except (ValueError, KeyError, TypeError, SyntaxError, RecursionError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
