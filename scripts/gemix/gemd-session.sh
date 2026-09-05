#!/usr/bin/env bash
set -euo pipefail

# Give one command a private GEM server. Logs never enter GDB's MI stream.
if (( $# == 0 )); then
    echo "usage: $0 command [args...]" >&2
    exit 2
fi
session_dir=$(mktemp -d /tmp/native-gemd.XXXXXXXX)
export GEMD_SOCKET="${session_dir}/gemd.sock"
server_pid=
cleanup() {
    local status=$?
    if [[ -n "${server_pid}" ]]; then
        kill "${server_pid}" 2>/dev/null || true
        wait "${server_pid}" 2>/dev/null || true
    fi
    if [[ -s "${session_dir}/server.log" ]]; then
        cat "${session_dir}/server.log" >&2
    fi
    # Only remove the three exact resources owned by this session.
    rm -f "${session_dir}/gemd.sock" "${session_dir}/server.log"
    rmdir "${session_dir}"
    exit "${status}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"${GEMD_BIN:-/opt/gemix/bin/gemd}" >"${session_dir}/server.log" 2>&1 &
server_pid=$!
for (( attempt=0; attempt<100; ++attempt )); do
    [[ -S "${GEMD_SOCKET}" ]] && break
    if ! kill -0 "${server_pid}" 2>/dev/null; then exit 1; fi
    sleep 0.05
done
[[ -S "${GEMD_SOCKET}" ]] || exit 1
"$@"
if ! kill -0 "${server_pid}" 2>/dev/null; then
    echo "gemd exited unexpectedly during the session" >&2
    exit 1
fi
