#!/usr/bin/env bash
# Shared connection settings; the UUID survives a libvirt domain rename.
set -euo pipefail
vm_script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
vm_workspace=$(cd "${vm_script_dir}/../../.." && pwd)
vm_domain=${WINDOWS_VM_DOMAIN:-95319e95-537b-4479-9b79-53419fe70fe9}
vm_user=${WINDOWS_VM_USER:-tomaz stih}
vm_remote_dir=C:/NativeDebug/native
vm_image=wischner/gcc-x86_64-windows-mingw-w64:latest
vm_key_alias=native-windows-vm
vm_ssh_options=(-o BatchMode=yes -o ConnectTimeout=5
    -o StrictHostKeyChecking=yes -o "HostKeyAlias=${vm_key_alias}"
    -o "UserKnownHostsFile=${HOME}/.ssh/native_windows_known_hosts")

vm_address() {
    vm_ip=${WINDOWS_VM_HOST:-}
    if [[ -z ${vm_ip} ]]; then
        vm_ip=$(virsh domifaddr "${vm_domain}" --source lease |
            awk '$3 == "ipv4" {split($4, ip, "/"); print ip[1]; exit}')
    fi
    [[ -n ${vm_ip} ]] || { echo 'Windows VM has no IPv4 lease yet.' >&2; return 1; }
}

# Encode the command instead of relying on cmd.exe/PowerShell shell quoting.
vm_powershell() {
    local encoded
    encoded=$(printf '%s' "\$ProgressPreference='SilentlyContinue'; \$ErrorActionPreference='Stop'; $1" |
        iconv -f UTF-8 -t UTF-16LE | base64 -w0)
    ssh "${vm_ssh_options[@]}" -l "${vm_user}" "${vm_ip}" \
        "powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ${encoded}"
}

vm_copy() {
    scp "${vm_ssh_options[@]}" -o "User=\"${vm_user}\"" "$@" \
        "${vm_ip}:${vm_remote_dir}/"
}
