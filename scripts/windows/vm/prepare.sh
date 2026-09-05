#!/usr/bin/env bash
# Deploy only the Docker-built program, runtime DLLs and native debug server.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
if [[ $(virsh domstate "${vm_domain}") != running ]]; then
    virsh start "${vm_domain}"
fi
ready=false
for ((attempt = 0; attempt < 45; ++attempt)); do
    if vm_address && vm_powershell 'exit 0' >/dev/null 2>&1; then
        ready=true
        break
    fi
    sleep 1
done
if [[ ${ready} != true ]]; then
    echo 'Windows SSH is unavailable. Complete the guest/key setup first.' >&2
    exit 1
fi
vm_powershell "\$ErrorActionPreference='Stop';
    New-Item -ItemType Directory -Force '${vm_remote_dir}' | Out-Null;
    if (Get-Process vision,gdbserver -ErrorAction SilentlyContinue | Where-Object {
        \$_.Path -like 'C:\NativeDebug\native\*'}) {
        throw 'Close the previous Native VM debug session before deploying.'
    }"
docker run --rm -u "$(id -u):$(id -g)" \
    -v "${vm_workspace}:${vm_workspace}" -w "${vm_workspace}" "${vm_image}" \
    bash -c 'set -e; output=build/windows-mingw-w64/src
        for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
            cp "$(x86_64-w64-mingw32-g++ -print-file-name="$dll")" "$output/"
        done
        cp /usr/share/win64/gdbserver.exe "$output/"'
vm_copy "${vm_workspace}/build/windows-mingw-w64/src/vision.exe" \
    "${vm_workspace}/build/windows-mingw-w64/src/libgcc_s_seh-1.dll" \
    "${vm_workspace}/build/windows-mingw-w64/src/libstdc++-6.dll" \
    "${vm_workspace}/build/windows-mingw-w64/src/libwinpthread-1.dll" \
    "${vm_workspace}/build/windows-mingw-w64/src/gdbserver.exe" \
    "${vm_script_dir}/session.ps1"
vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Install"
echo "Windows VM deployment ready at ${vm_ip}."
