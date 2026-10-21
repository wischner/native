#!/usr/bin/env bash
set -euo pipefail
# A real Wayland compositor for container runtime regressions, without X.
if (( $# == 0 )); then exit 2; fi
session_dir=$(mktemp -d /tmp/native-wayland.XXXXXXXX)
export XDG_RUNTIME_DIR=$session_dir
export WAYLAND_DISPLAY=wayland-native
export GDK_BACKEND=wayland
export GTK_A11Y=none
export GSK_RENDERER=cairo
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
compositor_pid=
cleanup() {
    local status=$?
    if [[ -n $compositor_pid ]]; then
        kill "$compositor_pid" 2>/dev/null || true
        wait "$compositor_pid" 2>/dev/null || true
    fi
    if (( status != 0 )); then cat "$session_dir/weston.log" >&2; fi
    rm -rf -- "$session_dir"
    exit "$status"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
weston --backend=headless-backend.so --renderer=pixman --socket="$WAYLAND_DISPLAY" \
    --width=1280 --height=1024 --idle-time=0 --log="$session_dir/weston.log" >/dev/null 2>&1 &
compositor_pid=$!
for (( attempt=0; attempt<100; ++attempt )); do
    kill -0 "$compositor_pid" 2>/dev/null || exit 1
    [[ -S $session_dir/$WAYLAND_DISPLAY ]] && break
    sleep .05
done
[[ -S $session_dir/$WAYLAND_DISPLAY ]] || exit 1
dbus-run-session -- "$@"
