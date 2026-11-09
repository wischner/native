#!/usr/bin/env python3
"""Package a resolved Native SDK and verify its relocated CMake interface.

Run inside the build SDK (or on the GitHub macOS runner). Never execute a
target binary: Windows and Haiku dependency scans use the cross objdump.
MIT License (see LICENSE). Copyright (C) 2026 Tomaz Stih.
"""

import argparse
import hashlib
import json
import re
import shlex
import shutil
import subprocess
import tarfile
from pathlib import Path


def run(*args):
    """Run a checked host tool without shell expansion."""
    return subprocess.check_output([str(arg) for arg in args], text=True).strip()


def cmake_quote(value):
    return '"' + str(value).replace('\\', '/').replace('"', '\\"') + '"'


def read_targets(build):
    reply = build / '.cmake/api/v1/reply'
    # CMake reads queries before evaluating CMakeLists.txt. On the first SDK
    # configure our query is new, so generate its reply before inspecting it.
    if not list(reply.glob('index-*.json')):
        subprocess.run(['cmake', str(build)], cwd=build, check=True)
    index = json.loads(max(reply.glob('index-*.json')).read_text())
    model = json.loads((reply / index['reply']['codemodel-v2']['jsonFile']).read_text())
    return {
        target['name']: json.loads((reply / target['jsonFile']).read_text())
        for target in model['configurations'][0]['targets']
    }


class Package:
    def __init__(self, build):
        self.build = build.resolve()
        self.info = json.loads((self.build / 'sdk-info.json').read_text())
        self.source = Path(self.info['source'])
        self.system = self.info['system']
        self.name = f"native-{self.info['version']}-{self.info['id']}-sdk"
        self.root = self.build / 'sdk-stage' / self.name
        if self.root.exists():
            shutil.rmtree(self.root)
        (self.root / 'lib').mkdir(parents=True)
        self.files = {}
        self.system_libraries = set()
        self.optional_missing = []
        self.scanned = set()
        self.search = []
        self.runtime = {}
        self.targets = read_targets(self.build)
        self.fragments = self.targets['native_sdk_probe']['link']['commandFragments']
        for fragment in self.fragments:
            if fragment['role'] == 'libraryPath':
                for token in shlex.split(fragment['fragment']):
                    if token.startswith('-L'):
                        self.search.append(Path(token[2:]))
        if self.system != 'Darwin':
            output = run(self.info['compiler'], '-print-search-dirs')
            for line in output.splitlines():
                if line.startswith('libraries: ='):
                    self.search.extend(Path(p) for p in line[12:].split(':') if p)
        if self.system == 'Linux':
            self.search.extend([Path('/usr/local/lib'), Path('/usr/openwin/lib')])
            for line in run('/sbin/ldconfig', '-p').splitlines():
                match = re.match(r'\s*(\S+) .* => (\S+)', line)
                if match and 'x86-64' in line:
                    self.runtime.setdefault(match[1], Path(match[2]))
        elif self.system == 'Windows':
            self.search.extend([Path('/usr/x86_64-w64-mingw32/bin'),
                                Path('/opt/native-winui/runtime')])
        elif self.system == 'Haiku':
            self.search.append(Path(self.info['sysroot']) / 'boot/system/lib')

    def is_system(self, name):
        """Leave OS-owned runtime components to the destination OS."""
        if self.system == 'Linux':
            result = bool(re.fullmatch(
                r'(ld-linux[^/]*|lib(c|m|pthread|dl|rt|util|resolv|nss_[^.]+)\.so(?:\..*)?)',
                name))
        elif self.system == 'Windows':
            result = name.lower().startswith(('api-ms-win-', 'ext-ms-win-')) or name.lower() in {
                'kernel32.dll', 'user32.dll', 'gdi32.dll', 'msimg32.dll',
                'advapi32.dll', 'shell32.dll', 'ole32.dll', 'oleaut32.dll',
                'gdiplus.dll', 'comctl32.dll', 'uxtheme.dll', 'opengl32.dll',
                'runtimeobject.dll', 'msvcrt.dll', 'ucrtbase.dll', 'ntdll.dll',
                'combase.dll', 'shcore.dll', 'shlwapi.dll', 'version.dll',
                'bcrypt.dll', 'rpcrt4.dll', 'ws2_32.dll', 'sechost.dll',
            }
        elif self.system == 'Haiku':
            os_lib = Path(self.info['sysroot']) / 'boot/system/lib' / name
            result = os_lib.is_file() or name in {'libroot.so', 'libnetwork.so'}
        else:
            result = name.startswith(('/System/Library/', '/usr/lib/'))
        if result:
            self.system_libraries.add(name)
        return result

    def resolve(self, name, directories=()):
        """Resolve a target library without falling back to host cross libraries."""
        for directory in [*directories, *self.search]:
            path = directory / name
            if path.is_file():
                return path
            if self.system == 'Windows' and directory.is_dir():
                for entry in directory.iterdir():
                    if entry.name.lower() == name.lower() and entry.is_file():
                        return entry
        if name in self.runtime:
            return self.runtime[name]
        path = Path(run(self.info['compiler'], f'-print-file-name={name}'))
        if path.is_absolute() and path.is_file():
            return path
        raise RuntimeError(f'Unresolved {self.system} dependency: {name}')

    def copy(self, source, relative):
        """Dereference SDK symlinks, detecting collisions in the flat library tree."""
        destination = self.root / relative
        digest = hashlib.sha256(source.read_bytes()).hexdigest()
        if relative in self.files and self.files[relative]['sha256'] != digest:
            raise RuntimeError(f'Conflicting dependency files: {relative}')
        destination.parent.mkdir(parents=True, exist_ok=True)
        # Keep bytes and permission bits, not host filesystem flags/xattrs.
        # macOS system notices can carry protected flags that an ordinary
        # GitHub runner cannot apply to files in the package staging tree.
        shutil.copy(source, destination)
        self.files[relative] = {'source': str(source), 'sha256': digest}
        return relative

    def library(self, path):
        directory = 'bin' if path.suffix.lower() == '.dll' else 'lib'
        relative = self.copy(path, f'{directory}/{path.name}')
        self.scan(path)
        return '${_native_prefix}/' + relative

    def scan(self, path):
        """Walk every ELF/PE import, including dependencies of direct libraries."""
        real = path.resolve()
        if real in self.scanned or path.suffix == '.a':
            return
        self.scanned.add(real)
        directories = []
        if self.system == 'Darwin':
            names = [line.strip().split(' (')[0]
                     for line in run('otool', '-L', path).splitlines()[1:]]
        else:
            output = run(self.info['objdump'], '-p', path)
            names = re.findall(r'^\s*(?:NEEDED|DLL Name:)\s+(\S+)', output, re.M)
            for value in re.findall(r'^\s*(?:RPATH|RUNPATH)\s+(\S+)', output, re.M):
                for directory in value.split(':'):
                    expanded = directory.replace('${ORIGIN}', str(real.parent))
                    expanded = expanded.replace('$ORIGIN', str(real.parent))
                    if expanded.startswith('/'):
                        directories.append(Path(expanded))
        # Haiku's sysroot holds OS link libraries, not a complete OS runtime.
        # Their ICU/compression/compiler runtimes belong to the installed OS;
        # never substitute the Linux host's identically named libraries.
        if self.system == 'Haiku' and real.is_relative_to(Path(self.info['sysroot'])):
            self.system_libraries.update(names)
            return
        for name in names:
            if self.is_system(name):
                continue
            dependency = Path(name) if name.startswith('/') else self.resolve(name, directories)
            self.library(dependency)

    def link_interface(self):
        """Keep CMake's link order and replace SDK paths with package paths."""
        links = []
        for fragment in self.fragments:
            if fragment['role'] != 'libraries':
                continue
            tokens = iter(shlex.split(fragment['fragment']))
            for token in tokens:
                if token in {'-Wl,--whole-archive', '-Wl,--no-whole-archive'}:
                    continue
                if token == '-framework':
                    framework = next(tokens)
                    links.append(f'-framework {framework}')
                    self.system_libraries.add(f'{framework}.framework')
                elif token.startswith('-l'):
                    name = token[2:]
                    if self.system == 'Linux' and name in {'dl', 'm', 'pthread', 'rt'}:
                        links.append(token)
                        self.system_libraries.add(token)
                        continue
                    candidates = ([name[1:]] if name.startswith(':') else
                                  [f'lib{name}.dll.a', f'lib{name}.a'] if self.system == 'Windows' else
                                  [f'lib{name}.so', f'lib{name}.a'])
                    for candidate in candidates:
                        try:
                            path = self.resolve(candidate)
                            break
                        except RuntimeError:
                            pass
                    else:
                        raise RuntimeError(f'Cannot package link dependency {token}')
                    links.append(self.library(path))
                elif token.startswith('-'):
                    links.append(token)
                else:
                    path = Path(token)
                    if not path.is_absolute():
                        path = self.build / path
                    if path.name != 'libnative.a':
                        links.append(self.library(path))
        return links

    def licenses(self):
        """Preserve notices from the build SDK and embedded third-party sources."""
        self.copy(self.source / 'LICENSE', 'LICENSE')
        roots = [self.source / 'lib/native/third_party', Path('/usr/share/doc'),
                 Path('/usr/share/common-licenses'), Path('/usr/local/share/doc'),
                 Path('/opt/native-winui/licenses'), Path('/opt/gemix/share')]
        for index, root in enumerate(roots):
            if not root.is_dir():
                continue
            for path in sorted(root.rglob('*')):
                if path.is_file() and (re.search(r'license|copying|copyright|notice', path.name, re.I)
                                       or root.name in {'common-licenses', 'licenses'}
                                       or path.name in {'README.md', 'stb_truetype.h'}):
                    self.copy(path, f'share/licenses/{index}/{path.relative_to(root)}')

    def config(self, links):
        directory = self.root / 'lib/cmake/Native'
        directory.mkdir(parents=True)
        text = [
            'get_filename_component(_native_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)',
            f'if(NOT CMAKE_SYSTEM_NAME STREQUAL "{self.system}")',
            '  message(FATAL_ERROR "This Native SDK targets a different operating system")',
            'endif()',
            'if(TARGET Native::Native)', '  return()', 'endif()',
            'add_library(Native::Native STATIC IMPORTED)',
            'set_target_properties(Native::Native PROPERTIES',
            '  IMPORTED_LOCATION "${_native_prefix}/lib/libnative.a"',
            '  INTERFACE_INCLUDE_DIRECTORIES "${_native_prefix}/include"',
            '  INTERFACE_COMPILE_FEATURES cxx_std_20)',
            'set_property(TARGET Native::Native PROPERTY INTERFACE_LINK_LIBRARIES',
            *['  ' + cmake_quote(link) for link in links], ')',
        ]
        if self.system in {'Linux', 'Haiku'}:
            text += ['set_property(TARGET Native::Native PROPERTY INTERFACE_LINK_OPTIONS',
                     '  "-Wl,-rpath-link,${_native_prefix}/lib")']
        if self.system == 'Windows':
            for name in ('visual_styles.rc', 'visual_styles.manifest'):
                self.copy(self.source / 'lib/native/platforms/windows' / name,
                          'share/native/' + name)
            text += ['enable_language(RC)',
                     'set_property(TARGET Native::Native PROPERTY INTERFACE_SOURCES',
                     '  "${_native_prefix}/share/native/visual_styles.rc")']
        if self.info['experimental_threads']:
            for prop in ('INTERFACE_COMPILE_OPTIONS', 'INTERFACE_LINK_OPTIONS'):
                text += [f'set_property(TARGET Native::Native APPEND PROPERTY {prop}',
                         '  -fexperimental-library)']
        text += ['unset(_native_prefix)', '']
        (directory / 'NativeConfig.cmake').write_text('\n'.join(text))

    def verify(self):
        """Link a fresh consumer after moving the SDK away from its staging path."""
        relocated = self.build / 'sdk-check' / self.name
        if relocated.parent.exists():
            shutil.rmtree(relocated.parent)
        relocated.parent.mkdir(parents=True)
        shutil.move(self.root, relocated)
        self.root = relocated
        source = relocated.parent / 'consumer'
        source.mkdir()
        shutil.copy2(self.source / 'tests/native_sdk_probe.cpp', source / 'main.cpp')
        (source / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.20)\n'
            'project(sdk_consumer LANGUAGES CXX)\n'
            'find_package(Native CONFIG REQUIRED)\n'
            'add_executable(sdk_consumer main.cpp)\n'
            'target_link_libraries(sdk_consumer PRIVATE Native::Native)\n')
        command = ['cmake', '-S', source, '-B', source.parent / 'build',
                   '-DCMAKE_BUILD_TYPE=Release', f'-DCMAKE_PREFIX_PATH={relocated}',
                   f"-DCMAKE_CXX_COMPILER={self.info['compiler']}"]
        if self.info['toolchain']:
            command.append(f"-DCMAKE_TOOLCHAIN_FILE={self.info['toolchain']}")
        if self.system == 'Haiku':
            command.append(f'-DNative_DIR={relocated}/lib/cmake/Native')
        subprocess.run([str(item) for item in command], check=True)
        subprocess.run(['cmake', '--build', str(source.parent / 'build'), '--parallel', '2'], check=True)

    def package(self):
        shutil.copytree(self.source / 'include', self.root / 'include')
        archive = self.build / self.targets['native']['artifacts'][0]['path']
        self.copy(archive, 'lib/libnative.a')
        links = self.link_interface()
        probe = self.build / self.targets['native_sdk_probe']['artifacts'][0]['path']
        self.scan(probe)
        if self.system == 'Windows':
            self.library(Path(self.info['bootstrap']))
        if self.system == 'Linux':
            # These optional services are loaded by name, not ELF DT_NEEDED.
            for name in ('libasound.so.2', 'libEGL.so.1', 'libGLX.so.0'):
                try:
                    dependency = self.resolve(name)
                except RuntimeError:
                    self.optional_missing.append(name)
                else:
                    self.library(dependency)
        self.licenses()
        self.config(links)
        self.verify()
        manifest = dict(self.info, files=self.files,
                        system_libraries=sorted(self.system_libraries),
                        optional_libraries_not_bundled=self.optional_missing,
                        compiler_version=run(self.info['compiler'], '--version'))
        (self.root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
        shutil.copy2(self.source / 'scripts/release/SDK-README.txt', self.root / 'README.txt')
        output = self.source / 'build/release'
        output.mkdir(parents=True, exist_ok=True)
        destination = output / (self.name + '.tar.gz')
        with tarfile.open(destination, 'w:gz') as tar:
            tar.add(self.root, arcname=self.name)
        digest = hashlib.sha256(destination.read_bytes()).hexdigest()
        destination.with_suffix(destination.suffix + '.sha256').write_text(
            f'{digest}  {destination.name}\n')
        print(f'Packaged and link-verified {destination}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    args = parser.parse_args()
    Package(args.build).package()


if __name__ == '__main__':
    main()
