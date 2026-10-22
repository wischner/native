#!/usr/bin/env python3
"""Install pinned WinUI metadata and generate MinGW C++ projections."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import urllib.request
import zipfile
import os

root = Path('/opt/native-winui')
root.mkdir(parents=True, exist_ok=True)
packages = (
    ('microsoft.windowsappsdk.winui', '1.8.250906003', '5792d98965490e8871b31c9c34ed9c6b0392da0a444c929abf93b1f4802b2126'),
    ('microsoft.windowsappsdk.foundation', '1.8.250906002', '77acb164613b71b55924e22f60da7c94f78ab9ebd426395f49e79ca3a0bea77c'),
    ('microsoft.windowsappsdk.base', '1.8.250831001', '76c20ad89f166ef204cd698648d1b160ded1d71607aeebcfb8303e1a539a4937'),
    ('microsoft.windowsappsdk.interactiveexperiences', '1.8.250906004', '454b682994bbbcf3f5029f42fa1aebff72b0bb29dde1ca6e1e99cae264c74451'),
    ('microsoft.windows.sdk.contracts', '10.0.26100.1', '933d0aee3bf2ec5ad04d2e19d27609a95eecec4d3ff0f50b222e2e3931819ef9'),
    ('microsoft.windows.cppwinrt', '3.0.260818.1', 'a8993608ae9263a8288e1a1d9ee2684a5632f4acc59e1e89ddd86b3b889a7367'),
    ('microsoft.web.webview2', '1.0.3179.45', '70bd381c5d67f97a6a80294001cde125ab5b24b6d36098ad901d8e15efd752d0'),
)

def download(url, path, checksum):
    urllib.request.urlretrieve(url, path)
    if hashlib.sha256(path.read_bytes()).hexdigest() != checksum:
        raise RuntimeError(f'SDK checksum mismatch: {path.name}')

for name, version, checksum in packages:
    archive = root / f'{name}.nupkg'
    download(f'https://api.nuget.org/v3-flatcontainer/{name}/{version}/{name}.{version}.nupkg', archive, checksum)
    with zipfile.ZipFile(archive) as package:
        for entry in sorted(package.namelist()):
            destination = None
            if entry.endswith('.winmd') and not entry.startswith('tools/'):
                destination = root / ('contracts' if name.endswith('contracts') else 'metadata') / Path(entry).name
            elif entry.endswith('cppwinrt.exe'):
                destination = root / 'cppwinrt.exe'
            elif entry.endswith('win-x64/native/Microsoft.WindowsAppRuntime.Bootstrap.dll'):
                destination = root / 'runtime' / Path(entry).name
            elif entry.endswith(('.h', '.hpp')) and entry.startswith('include/'):
                destination = root / entry
            elif 'license' in entry.lower() and not entry.endswith('/'):
                destination = root / 'licenses' / name / Path(entry).name
            if destination:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(package.read(entry))
    archive.unlink()

archive = root / 'cppwinrt.tar.zst'
download('https://mirror.msys2.org/mingw/mingw64/mingw-w64-x86_64-cppwinrt-3.0.260818.1-3-any.pkg.tar.zst', archive,
         '44f201b9c63cd34f01abe8a04f8de70b609e7db803d9303982a2cf89a24e335d')
subprocess.run(['tar', '--zstd', '-xf', str(archive), '-C', str(root)], check=True)
shutil.copytree(root / 'mingw64/include/winrt', root / 'include/winrt', dirs_exist_ok=True)
shutil.rmtree(root / 'mingw64')
archive.unlink()
environment = dict(os.environ, WINEDEBUG='-all', WINEPREFIX=str(root / 'wine'),
                   WINEDLLOVERRIDES='mscoree,mshtml=d')
subprocess.run(['/usr/lib/wine/wine64', str(root / 'cppwinrt.exe'),
                '-input', r'Z:\opt\native-winui\metadata',
                '-reference', r'Z:\opt\native-winui\contracts',
                '-output', r'Z:\opt\native-winui\include'], env=environment, check=True)
shutil.rmtree(root / 'wine')
(root / 'cppwinrt.exe').unlink()
