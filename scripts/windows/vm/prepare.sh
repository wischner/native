#!/usr/bin/env bash
# Deploy only the Docker-built program, runtime DLLs and native debug server.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
if [[ -n ${vm_domain} ]] && [[ $(virsh domstate "${vm_domain}") != running ]]; then
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
vm_powershell 'if (!(Get-AppxPackage -Name Microsoft.WindowsAppRuntime.1.8 | Where-Object { $_.Architecture -eq "X64" })) { throw "Install Windows App Runtime 1.8 (x64) before using the WinUI settings navigator." }' >/dev/null
mkdir -p "${vm_workspace}/build/windows-vm"
docker run --rm -u "$(id -u):$(id -g)" \
    -v "${vm_workspace}:${vm_workspace}" -w "${vm_workspace}" "${vm_image}" \
    bash -c 'set -e; output=build/windows-mingw-w64/src
        for dll in libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll; do
            cp "$(x86_64-w64-mingw32-g++ -print-file-name="$dll")" "$output/"
        done
        cp /opt/native-winui/runtime/Microsoft.WindowsAppRuntime.Bootstrap.dll "$output/"
        cp /usr/share/win64/gdbserver.exe "$output/"
        x86_64-w64-mingw32-objcopy --strip-debug "$output/vision.exe" build/windows-vm/vision.exe'
sync_options=()
for ((index = 0; index < ${#vm_ssh_options[@]}; index += 2)); do
    sync_options+=("--ssh-option=${vm_ssh_options[index + 1]}")
done
python3 "${vm_script_dir}/sync.py" "${vm_ip}" "${sync_options[@]}"
if [[ ${WINDOWS_VM_VERBOSE:-0} == 1 ]]; then
    echo "Windows VM deployment ready at ${vm_ip}."
fi
