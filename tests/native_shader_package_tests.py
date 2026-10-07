#!/usr/bin/env python3
"""Exercise portable effect compilation without evaluating manifest code.

MIT License, Copyright (C) 2026 Tomaz Stih.
"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("shader_package", ROOT / "scripts/shaders/package.py")
PACKAGE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PACKAGE)


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.manifest = json.loads((ROOT / "tests/data/shaders/crt.json").read_text())

    def test_reproducible_crt_package(self):
        self.assertEqual(PACKAGE.compile_package(self.manifest),
                         (ROOT / "tests/data/shaders/crt.nshader").read_text())

    def test_reproducible_terminal_package(self):
        assets = ROOT / "src/assets/retro-terminal"
        manifest = json.loads((assets / "terminal.json").read_text())
        self.assertEqual(PACKAGE.compile_package(manifest),
                         (assets / "terminal.nshader").read_text())

    def test_integer_defaults_preserve_precision(self):
        self.manifest["parameters"].append({
            "name": "count", "type": "int", "range": [-2147483648, 2147483648],
            "default": 2147483647})
        compiled = PACKAGE.compile_package(self.manifest)
        self.assertIn("2147483647\n", compiled)

    def test_integer_defaults_reject_booleans(self):
        self.manifest["parameters"][0] = {
            "name": "count", "type": "int", "range": [0, 1], "default": True}
        with self.assertRaises(ValueError):
            PACKAGE.compile_package(self.manifest)

    def test_rejects_code_and_forward_dependencies(self):
        for expression in ("__import__('os').system('false')", "[x for x in uv]",
                           "sample(bloom, uv)", "uv[0]", "lambda: 1"):
            manifest = copy.deepcopy(self.manifest)
            manifest["passes"][0]["program"] = [["result", expression]]
            manifest["passes"][0]["output"] = "result"
            with self.subTest(expression=expression), self.assertRaises(ValueError):
                PACKAGE.compile_package(manifest)

    def test_rejects_invalid_defaults_and_reserved_names(self):
        for change in ({"name": "uv"}, {"default": 1000000}, {"default": float('nan')}):
            manifest = copy.deepcopy(self.manifest)
            manifest["parameters"][0].update(change)
            with self.subTest(change=change), self.assertRaises(ValueError):
                PACKAGE.compile_package(manifest)


if __name__ == "__main__":
    unittest.main()
