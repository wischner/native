#!/usr/bin/env bash
# Own one desktop debug server and its SSH tunnel for one cppdbg connection.
# Reserve stdout exclusively for GDB/MI.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
vm_address
exec 9>"${vm_workspace}/build/windows-vm-debug.lock"
flock -n 9 || { echo 'A Windows VM debug session is already active.' >&2; exit 1; }
tunnel_started=false
started=false
cleanup() {
    if [[ ${started} == true ]]; then
        vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Stop" >&2 || true
    fi
    if [[ ${tunnel_started} == true ]]; then
        vm_stop_tunnel || true
    fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM HUP
# Recover only this workspace's previous tunnel, after acquiring its lock.
vm_stop_tunnel
# -f returns only after authentication and forwarding have succeeded. Close
# the lock descriptor in the master so a forced launcher exit cannot strand it.
ssh "${vm_ssh_options[@]}" -M -S "${vm_tunnel_socket}" -f -N \
    -o ControlPersist=no -o ExitOnForwardFailure=yes -o ServerAliveInterval=10 \
    -L 127.0.0.1:31337:127.0.0.1:2345 "${vm_ip}" 9>&- >&2
tunnel_started=true
started=true
vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Start" >&2
cache=${vm_workspace}/build/windows-vm/libraries
symbols=(-iex "set sysroot target:")
if [[ -f ${cache}/version.txt ]]; then
    symbols=(-iex "set target-file-system-kind dos-based"
        -iex "set sysroot ${cache}"
        -iex "set solib-search-path ${vm_workspace}/build/windows-mingw-w64/src:${cache}")
fi
"${vm_workspace}/scripts/linux/docker-debug.sh" "${vm_image}" \
    /usr/bin/x86_64-w64-mingw32-gdb "${symbols[@]}" "$@"
