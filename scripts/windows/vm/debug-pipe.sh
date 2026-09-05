#!/usr/bin/env bash
# Own one desktop debug server and its SSH tunnel for one cppdbg connection.
# Reserve stdout exclusively for GDB/MI.
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/common.sh"
vm_address
exec 9>"${vm_workspace}/build/windows-vm-debug.lock"
flock -n 9 || { echo 'A Windows VM debug session is already active.' >&2; exit 1; }
tunnel_pid=
started=false
cleanup() {
    if [[ ${started} == true ]]; then
        vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Stop" >&2 || true
    fi
    if [[ -n ${tunnel_pid} ]]; then
        kill "${tunnel_pid}" 2>/dev/null || true
        wait "${tunnel_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM
ssh "${vm_ssh_options[@]}" -l "${vm_user}" -N \
    -o ExitOnForwardFailure=yes -o ServerAliveInterval=10 \
    -L 127.0.0.1:31337:127.0.0.1:2345 "${vm_ip}" >&2 &
tunnel_pid=$!
vm_powershell "& '${vm_remote_dir}/session.ps1' -Action Start" >&2
started=true
kill -0 "${tunnel_pid}" 2>/dev/null || { echo 'SSH tunnel failed.' >&2; exit 1; }
"${vm_workspace}/scripts/linux/docker-debug.sh" "${vm_image}" \
    /usr/bin/x86_64-w64-mingw32-gdb "$@"
