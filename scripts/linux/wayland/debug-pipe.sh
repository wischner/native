#!/usr/bin/env bash
set -euo pipefail
# Forward only the Wayland socket. No X11 socket or Xauthority is mounted.
if (( $# < 2 )); then
    echo "usage: $0 image debugger [arguments...]" >&2
    exit 2
fi
image=$1
shift
command=("$@")
if (( ${#command[@]} == 1 )); then
    read -r -a command <<< "${command[0]}"
fi
workspace=$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)
socket=${WAYLAND_DISPLAY:-wayland-0}
if [[ $socket != /* ]]; then socket=${XDG_RUNTIME_DIR:?Wayland requires XDG_RUNTIME_DIR}/$socket; fi
if [[ ! -S $socket ]]; then
    echo "Wayland compositor socket not found: $socket" >&2
    exit 1
fi
bus_args=()
if [[ -S ${XDG_RUNTIME_DIR:-}/bus ]]; then
    bus_args+=(-v "$XDG_RUNTIME_DIR/bus:/run/native/bus"
        -e DBUS_SESSION_BUS_ADDRESS=unix:path=/run/native/bus)
fi
if [[ -S /run/dbus/system_bus_socket ]]; then
    bus_args+=(-v /run/dbus/system_bus_socket:/run/native/system-bus
        -e DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/native/system-bus)
fi
exec docker run --rm -i --cap-add SYS_PTRACE --security-opt seccomp=unconfined \
    -u "$(id -u):$(id -g)" -v "$workspace:$workspace" -w "$workspace" \
    --tmpfs "/run/native:rw,mode=0700,uid=$(id -u),gid=$(id -g)" \
    -v "$socket:/run/native/wayland-0" \
    -e XDG_RUNTIME_DIR=/run/native -e WAYLAND_DISPLAY=wayland-0 \
    -e GDK_BACKEND=wayland -e ASAN_OPTIONS=detect_leaks=0 \
    "${bus_args[@]}" "$image" "${command[@]}"
