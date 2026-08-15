#!/usr/bin/env bash
set -euo pipefail

# Start WineDbg's GDB proxy and keep it alive for one VS Code connection.
if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $0 <windows-program> [port]" >&2
    exit 2
fi

PROGRAM=$1
PORT=${2:-31337}

if [[ ! -f "${PROGRAM}" ]]; then
    echo "missing Windows program: ${PROGRAM}" >&2
    exit 1
fi

program_pids() {
    ps -eo pid=,args= | while read -r process_pid command; do
        if [[ "${command}" == "${PROGRAM}" ]]; then
            echo "${process_pid}"
        fi
    done
}

debug_server_pids() {
    local expected_suffix="winedbg.exe --gdb --no-start --port ${PORT} ${PROGRAM}"

    ps -eo pid=,args= | while read -r process_pid command; do
        if [[ "${command}" == *"${expected_suffix}" ]]; then
            echo "${process_pid}"
        fi
    done
}

mapfile -t EXISTING_PROGRAM_PIDS < <(program_pids)
mapfile -t EXISTING_WINEDBG_PIDS < <(debug_server_pids)
LOG_FILE=$(mktemp)
WINEDBG_PID=
cleanup() {
    local status=$?
    local process_pid debug_server_pid existing_pid
    local is_existing

    if [[ -n "${WINEDBG_PID}" ]] && kill -0 "${WINEDBG_PID}" 2>/dev/null; then
        kill "${WINEDBG_PID}" 2>/dev/null || true
        wait "${WINEDBG_PID}" 2>/dev/null || true
    fi

    # Wine reparents its Windows-side debugger, so it is not necessarily a
    # child of the launcher above. Stop only the proxy created by this task.
    while read -r debug_server_pid; do
        is_existing=false
        for existing_pid in "${EXISTING_WINEDBG_PIDS[@]}"; do
            if [[ "${debug_server_pid}" == "${existing_pid}" ]]; then
                is_existing=true
                break
            fi
        done
        if [[ "${is_existing}" == false ]]; then
            kill "${debug_server_pid}" 2>/dev/null || true
        fi
    done < <(debug_server_pids)

    # A detached remote target can outlive GDB. Stop only the instance that
    # appeared after this debug server started.
    while read -r process_pid; do
        is_existing=false
        for existing_pid in "${EXISTING_PROGRAM_PIDS[@]}"; do
            if [[ "${process_pid}" == "${existing_pid}" ]]; then
                is_existing=true
                break
            fi
        done
        if [[ "${is_existing}" == false ]]; then
            kill "${process_pid}" 2>/dev/null || true
        fi
    done < <(program_pids)

    rm -f "${LOG_FILE}"
    exit "${status}"
}
trap cleanup EXIT INT TERM

if ss -ltn "sport = :${PORT}" | tail -n +2 | grep -q .; then
    echo "TCP port ${PORT} is already in use." >&2
    exit 1
fi

echo "Starting Wine GDB proxy for ${PROGRAM} on localhost:${PORT}..."
WINEDEBUG=-all winedbg \
    --gdb \
    --no-start \
    --port "${PORT}" \
    "${PROGRAM}" \
    >"${LOG_FILE}" 2>&1 &
WINEDBG_PID=$!

for _ in $(seq 1 100); do
    if ! kill -0 "${WINEDBG_PID}" 2>/dev/null; then
        wait "${WINEDBG_PID}" || true
        sed -n '1,120p' "${LOG_FILE}" >&2
        echo "WineDbg exited before its GDB proxy became ready." >&2
        exit 1
    fi

    if ss -ltn "sport = :${PORT}" | tail -n +2 | grep -q .; then
        echo "Wine GDB proxy ready at localhost:${PORT}."
        wait "${WINEDBG_PID}"
        exit $?
    fi

    sleep 0.1
done

sed -n '1,120p' "${LOG_FILE}" >&2
echo "WineDbg did not open TCP port ${PORT}." >&2
exit 1
