#!/usr/bin/env python3
# Synchronize changed debug artifacts and cache the VM's loaded Windows DLLs.
# MIT License (see: LICENSE)
# Copyright (C) 2026 Tomaz Stih.
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest().upper()


def literal(value):
    return "'" + str(value).replace("'", "''") + "'"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('host')
    parser.add_argument('--ssh-option', action='append', default=[])
    args = parser.parse_args()
    options = [item for value in args.ssh_option for item in ('-o', value)]
    root = Path(__file__).resolve().parents[3]
    staging = root / 'build/windows-vm'
    staging.mkdir(parents=True, exist_ok=True)
    remote = 'C:/NativeDebug/native'

    def powershell(script):
        script = "$ProgressPreference='SilentlyContinue'; $ErrorActionPreference='Stop'; " + script
        encoded = base64.b64encode(script.encode('utf-16le')).decode()
        return subprocess.check_output(['ssh', *options, args.host,
            'powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ' + encoded], text=True)

    def copy(source, destination):
        subprocess.run(['scp', *options, str(source), str(destination)], check=True)

    files = {'vision.exe': staging / 'vision.exe'}
    for name in ('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll',
                 'Microsoft.WindowsAppRuntime.Bootstrap.dll', 'gdbserver.exe'):
        files[name] = root / 'build/windows-mingw-w64/src' / name
    for name in ('session.ps1', 'session.vbs'):
        files[name] = Path(__file__).parent / name
    assets = root / 'build/windows-mingw-w64/src/retro-terminal'
    files.update({str(path.relative_to(assets.parent)): path for path in assets.rglob('*') if path.is_file()})
    names = ','.join(literal(name) for name in files)
    status = json.loads(powershell(f"""
        New-Item -ItemType Directory -Force '{remote}' | Out-Null;
        if (Get-Process vision,gdbserver -ErrorAction SilentlyContinue | Where-Object {{ $_.Path -like 'C:\\NativeDebug\\native\\*' }}) {{ throw 'Close the previous Native debug session before deploying.' }};
        $files=@{{}}; foreach ($name in @({names})) {{ $path=Join-Path '{remote}' $name; if (Test-Path -LiteralPath $path -PathType Leaf) {{ $files[$name]=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }} }};
        $version=Get-ItemProperty 'HKLM:\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion';
        $stamp="$($version.CurrentBuildNumber).$($version.UBR)";
        $libraries=@(); if ((Test-Path '{remote}/gdb-libraries/version.txt') -and (Get-Content '{remote}/gdb-libraries/version.txt') -eq $stamp) {{
            $libraries=@(Get-ChildItem '{remote}/gdb-libraries/*.dll' | ForEach-Object {{ @{{Name=$_.Name;Hash=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}} }})
        }};
        @{{Files=$files;Libraries=$libraries;Version=$stamp;Task=[bool](Get-ScheduledTask -TaskName Native-Vision-Debug -ErrorAction SilentlyContinue)}} | ConvertTo-Json -Depth 5 -Compress
    """))
    changed = [name for name, path in files.items() if digest(path) != status['Files'].get(name)]
    if changed:
        archive = staging / 'update.tar'
        with tarfile.open(archive, 'w') as bundle:
            for name in changed:
                bundle.add(files[name], arcname=name)
        copy(archive, f'{args.host}:{remote}/update.tar')
        powershell(f"& tar.exe -xf '{remote}/update.tar' -C '{remote}'; if ($LASTEXITCODE) {{ throw 'Deployment extraction failed' }}; Remove-Item '{remote}/update.tar'")
    if not status['Task'] or any(name in changed for name in ('session.ps1', 'session.vbs')):
        powershell(f"& '{remote}/session.ps1' -Action Install")
    if os.environ.get('WINDOWS_VM_VERBOSE') == '1':
        print(f'Windows deployment: {len(changed)} changed files transferred.')

    libraries = staging / 'libraries'
    libraries.mkdir(exist_ok=True)
    version_file = libraries / 'version.txt'
    if not version_file.exists() or version_file.read_text() != status['Version']:
        for path in libraries.iterdir():
            if path.is_file() or path.is_symlink():
                path.unlink()
    missing = [entry['Name'] for entry in status['Libraries']
               if not (libraries / entry['Name']).is_file() or digest(libraries / entry['Name']) != entry['Hash']]
    if missing:
        names = ','.join(literal(name) for name in missing)
        powershell(f"& tar.exe -cf '{remote}/libraries.tar' -C '{remote}/gdb-libraries' @({names}); if ($LASTEXITCODE) {{ throw 'Library archive failed' }}")
        archive = staging / 'libraries.tar'
        copy(f'{args.host}:{remote}/libraries.tar', archive)
        with tarfile.open(archive) as bundle:
            for member in bundle:
                if member.name not in missing or not member.isfile():
                    raise RuntimeError('Unexpected library archive entry')
                with bundle.extractfile(member) as source:
                    (libraries / member.name).write_bytes(source.read())
        powershell(f"Remove-Item '{remote}/libraries.tar'")
    for entry in status['Libraries']:
        path = libraries / entry['Name']
        if digest(path) != entry['Hash']:
            raise RuntimeError('Cached DLL hash mismatch: ' + entry['Name'])
        for name in (path.name.lower(), path.name.upper()):
            alias = libraries / name
            if not alias.exists():
                alias.symlink_to(path.name)
    # Windows reports these common GUI DLLs with mixed case in debug events,
    # although module enumeration returns different casing. Linux lookup is
    # case sensitive; all aliases point to the same hash-verified DLL bytes.
    reported_names = ('KernelBase.dll', 'GdiPlus.dll', 'DXCore.dll',
                      'SHCore.dll', 'TextInputFramework.dll')
    by_name = {entry['Name'].lower(): entry['Name'] for entry in status['Libraries']}
    for name in reported_names:
        source = by_name.get(name.lower())
        alias = libraries / name
        if source and not alias.exists():
            alias.symlink_to(source)
    if status['Libraries']:
        version_file.write_text(status['Version'])
    if os.environ.get('WINDOWS_VM_VERBOSE') == '1':
        print(f"Windows DLL cache: {len(status['Libraries'])} files, {len(missing)} downloaded.")


if __name__ == '__main__':
    main()
