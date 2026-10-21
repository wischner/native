#!/usr/bin/env bash
# Run GDB in the VM's existing desktop. Keep stdout exclusively for GDB/MI.
set -euo pipefail
host=${ELEMENTARY_VM_HOST:-echo}
command=("$@")
if (( ${#command[@]} == 0 )); then command=(/usr/bin/gdb); fi
if (( ${#command[@]} == 1 )); then
    read -r -a command <<< "${command[0]}"
fi
printf -v debugger ' %q' "${command[@]}"
remote='runtime=/run/user/$(id -u)
export XDG_RUNTIME_DIR=$runtime
export WAYLAND_DISPLAY=wayland-0
export DBUS_SESSION_BUS_ADDRESS=unix:path=$runtime/bus
export GDK_BACKEND=wayland GSK_RENDERER=cairo ASAN_OPTIONS=detect_leaks=0
export LD_LIBRARY_PATH=/home/tomaz/Projects/native/run/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}
unset DISPLAY
if ! test -S "$runtime/$WAYLAND_DISPLAY"; then
    echo "The elementary VM needs an active Wayland desktop." >&2
    exit 1
fi
exec'
remote+=$debugger
printf -v quoted '%q' "$remote"
exec ssh -T -o BatchMode=yes "$host" "bash -c $quoted"
