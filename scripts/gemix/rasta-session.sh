#!/usr/bin/env bash
set -euo pipefail

# Container-owned SDL2 viewer and framebuffer; keep GDB/MI stdout clean.
if (( $# == 0 )); then
    echo "usage: $0 command [args...]" >&2
    exit 2
fi
session_dir=$(mktemp -d /tmp/native-rasta.XXXXXXXX)
export GEM_RESOURCE_DIR=${GEM_RESOURCE_DIR:-/opt/gemix/share/gem}
export GEM_VDI_WIDTH=904 GEM_VDI_HEIGHT=900
export GEM_RASTA_FRAMEBUFFER="${session_dir}/rasta.fb"
export GEM_RASTA_HOST=127.0.0.1 GEM_RASTA_PORT=5000
export GEM_RASTA_CURSOR=off GEM_RASTA_INVERSE=off
export ASAN_OPTIONS=detect_leaks=0
viewer_pid=
session_pid=
cleanup() {
    local status=$?
    trap - EXIT INT TERM
    if [[ -n "${session_pid}" ]]; then
        # The debugger, server and inferior belong to this session, not
        # to the caller's process group or another running GEM instance.
        pkill -TERM -s "${session_pid}" 2>/dev/null || true
        for (( attempt=0; attempt<20; ++attempt )); do
            if ! pkill -0 -s "${session_pid}" 2>/dev/null; then break; fi
            sleep 0.05
        done
        pkill -KILL -s "${session_pid}" 2>/dev/null || true
        wait "${session_pid}" 2>/dev/null || true
    fi
    if [[ -n "${viewer_pid}" ]]; then
        kill "${viewer_pid}" 2>/dev/null || true
        wait "${viewer_pid}" 2>/dev/null || true
    fi
    if [[ -s "${session_dir}/viewer.log" ]]; then
        cat "${session_dir}/viewer.log" >&2
    fi
    rm -f "${GEM_RASTA_FRAMEBUFFER}" "${session_dir}/viewer.log"
    rmdir "${session_dir}"
    exit "${status}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
/opt/gemix/bin/rasta --width 904 --height 900 --bpp 32 \
    --framebuffer "${GEM_RASTA_FRAMEBUFFER}" --port "${GEM_RASTA_PORT}" \
    --scale 1 --cursor off --inverse off >"${session_dir}/viewer.log" 2>&1 &
viewer_pid=$!
for (( attempt=0; attempt<100; ++attempt )); do
    kill -0 "${viewer_pid}" 2>/dev/null || exit 1
    [[ -s "${GEM_RASTA_FRAMEBUFFER}" ]] && break
    sleep 0.05
done
[[ -s "${GEM_RASTA_FRAMEBUFFER}" ]] || exit 1
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# Async jobs otherwise inherit /dev/null as stdin. GDB/MI must retain the
# original pipe, while its process session lets cleanup reach the inferior.
setsid bash "${SCRIPT_DIR}/gemd-session.sh" "$@" <&0 &
session_pid=$!
status=0
wait -n "${viewer_pid}" "${session_pid}" || status=$?
exit "${status}"
