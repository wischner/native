#!/usr/bin/env python3
"""Regression checks for SDK dependency closure and relocation boundaries.

MIT License (see LICENSE). Copyright (C) 2026 Tomaz Stih.
"""

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location(
    'package_sdk', ROOT / 'scripts/release/package_sdk.py')
SDK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SDK)


class PackageTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build').mkdir(exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=ROOT / 'build')
        self.addCleanup(self.temp.cleanup)
        base = Path(self.temp.name)
        self.pkg = SDK.Package.__new__(SDK.Package)
        self.pkg.root = base / 'sdk'
        self.pkg.build = base
        self.pkg.system = 'Linux'
        self.pkg.info = {'compiler': 'target-c++', 'objdump': 'target-objdump'}
        self.pkg.files = {}
        self.pkg.scanned = set()
        self.pkg.system_libraries = set()
        self.pkg.search = [base]
        self.pkg.runtime = {}

    def library(self, name, content=b'library'):
        path = self.pkg.build / name
        path.write_bytes(content)
        return path

    def test_recursive_closure_and_cycle(self):
        first = self.library('libfirst.so')
        self.library('libsecond.so.1')
        def inspect(*args):
            self.assertEqual(args[0], 'target-objdump')
            return ('  NEEDED libsecond.so.1\n  NEEDED libc.so.6' if args[-1] == first
                    else '  NEEDED libfirst.so')
        with patch.object(SDK, 'run', side_effect=inspect):
            self.pkg.library(first)
        self.assertEqual(set(self.pkg.files), {'lib/libfirst.so', 'lib/libsecond.so.1'})
        self.assertEqual(self.pkg.system_libraries, {'libc.so.6'})

    def test_unresolved_dependency_fails(self):
        with patch.object(SDK, 'run', return_value='libmissing.so'):
            with self.assertRaisesRegex(RuntimeError, 'Unresolved Linux'):
                self.pkg.resolve('libmissing.so')

    def test_elf_origin_runtime_search_path(self):
        first = self.library('libfirst.so')
        private = self.pkg.build / 'private/libprivate.so'
        private.parent.mkdir()
        private.write_bytes(b'private runtime')
        def inspect(*args):
            return ('  RUNPATH $ORIGIN/private\n  NEEDED libprivate.so'
                    if args[-1] == first else '')
        with patch.object(SDK, 'run', side_effect=inspect):
            self.pkg.library(first)
        self.assertIn('lib/libprivate.so', self.pkg.files)

    def test_colliding_flat_names_fail(self):
        first = self.library('one', b'one')
        second = self.library('two', b'two')
        self.pkg.copy(first, 'lib/conflict.so')
        with self.assertRaisesRegex(RuntimeError, 'Conflicting'):
            self.pkg.copy(second, 'lib/conflict.so')

    def test_symlinks_are_self_contained(self):
        original = self.library('libactual.so.1')
        alias = self.pkg.build / 'libactual.so'
        alias.symlink_to(original)
        self.pkg.copy(alias, 'lib/libactual.so')
        result = self.pkg.root / 'lib/libactual.so'
        self.assertFalse(result.is_symlink())
        self.assertEqual(result.read_bytes(), original.read_bytes())

    def test_windows_imports_are_case_insensitive(self):
        self.pkg.system = 'Windows'
        library = self.library('libwinpthread-1.dll')
        self.assertEqual(self.pkg.resolve('LIBWINPTHREAD-1.DLL'), library)
        self.assertTrue(self.pkg.is_system('KERNEL32.dll'))
        self.assertFalse(self.pkg.is_system('missing-third-party.dll'))

    def test_haiku_os_dependency_does_not_use_linux_runtime(self):
        self.pkg.system = 'Haiku'
        self.pkg.info['sysroot'] = str(self.pkg.build / 'sysroot')
        library = self.pkg.build / 'sysroot/boot/system/lib/libbe.so'
        library.parent.mkdir(parents=True)
        library.write_bytes(b'Haiku OS library')
        with patch.object(SDK, 'run', return_value='  NEEDED libstdc++.so.6'):
            self.pkg.library(library)
        self.assertEqual(self.pkg.system_libraries, {'libstdc++.so.6'})
        self.assertEqual(set(self.pkg.files), {'lib/libbe.so'})

    def test_link_order_and_relocation(self):
        self.library('libfirst.a')
        self.library('libsecond.a')
        self.pkg.fragments = [{'role': 'libraries', 'fragment':
            '-Wl,--whole-archive lib/native/libnative.a -Wl,--no-whole-archive '
            '-lfirst -lsecond -lfirst -ldl'}]
        with patch.object(SDK, 'run', side_effect=lambda *args: args[-1].split('=')[-1]):
            links = self.pkg.link_interface()
        self.assertEqual(links, ['${_native_prefix}/lib/libfirst.a',
                                 '${_native_prefix}/lib/libsecond.a',
                                 '${_native_prefix}/lib/libfirst.a', '-ldl'])


if __name__ == '__main__':
    unittest.main()
