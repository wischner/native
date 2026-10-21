#!/usr/bin/env bash
# Use the configured SSH alias for the Windows 11 desktop VM.
set -euo pipefail
vm_script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
vm_workspace=$(cd "${vm_script_dir}/../../.." && pwd)
vm_domain=${WINDOWS_VM_DOMAIN:-}
vm_remote_dir=C:/NativeDebug/native
vm_tunnel_socket=${vm_workspace}/build/windows-vm-ssh.sock
vm_image=wischner/gcc-x86_64-windows-mingw-w64:latest
# Suppress only the non-PQ advisory; retain host-key checks and other warnings.
# Older SSH clients ignore the newer option.
vm_ssh_options=(-o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=yes
    -o IgnoreUnknown=WarnWeakCrypto -o WarnWeakCrypto=no-pq-kex)
if [[ -n ${WINDOWS_VM_USER:-} ]]; then
    vm_ssh_options+=(-o "User=${WINDOWS_VM_USER}")
fi

vm_address() {
    vm_ip=${WINDOWS_VM_HOST:-whiskey}
}

# Encode the command instead of relying on cmd.exe/PowerShell shell quoting.
vm_powershell() {
    local encoded
    encoded=$(printf '%s' "\$ProgressPreference='SilentlyContinue'; \$ErrorActionPreference='Stop'; $1" |
        iconv -f UTF-8 -t UTF-16LE | base64 -w0)
    ssh "${vm_ssh_options[@]}" "${vm_ip}" \
        "powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ${encoded}"
}

vm_copy() {
    scp "${vm_ssh_options[@]}" "$@" \
        "${vm_ip}:${vm_remote_dir}/"
}

# The dedicated control socket lets post-debug cleanup recover an orphaned
# tunnel without touching other SSH connections or port owners.
vm_stop_tunnel() {
    if [[ -S ${vm_tunnel_socket} ]]; then
        ssh "${vm_ssh_options[@]}" -S "${vm_tunnel_socket}" \
            -O exit "${vm_ip}" >&2 || {
                # A killed master can leave an unreachable socket behind.
                rm -f -- "${vm_tunnel_socket}"
            }
    fi
}
