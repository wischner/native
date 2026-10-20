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
export GEM_RASTA_CURSOR=off GEM_RASTA_INVERSE=on
export ASAN_OPTIONS=detect_leaks=0
viewer_pid=
cleanup() {
    local status=$?
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
/opt/gemix/bin/rasta --width 904 --height 900 --bpp 1 \
    --framebuffer "${GEM_RASTA_FRAMEBUFFER}" --port "${GEM_RASTA_PORT}" \
    --scale 1 --cursor off --inverse on >"${session_dir}/viewer.log" 2>&1 &
viewer_pid=$!
for (( attempt=0; attempt<100; ++attempt )); do
    kill -0 "${viewer_pid}" 2>/dev/null || exit 1
    [[ -s "${GEM_RASTA_FRAMEBUFFER}" ]] && break
    sleep 0.05
done
[[ -s "${GEM_RASTA_FRAMEBUFFER}" ]] || exit 1
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
bash "${SCRIPT_DIR}/gemd-session.sh" "$@"
