#!/usr/bin/env bash
set -euo pipefail

# Start the local Rasta display and run VS Code's debugger in the GEMix image.
if [[ $# -lt 2 ]]; then
    echo "usage: $0 <docker-image> <debugger> [debugger-args...]" >&2
    exit 2
fi

IMAGE=$1
shift
GEMD_MODE=0
if [[ ${1:-} == --gemd ]]; then
    GEMD_MODE=1
    shift
fi

# cppdbg may append its MI option to debuggerPath and pass the resulting
# command as one argument. Docker needs the executable and its options as
# separate arguments.
DEBUGGER_COMMAND=("$@")
if (( ${#DEBUGGER_COMMAND[@]} == 1 )); then
    read -r -a DEBUGGER_COMMAND <<< "${DEBUGGER_COMMAND[0]}"
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
WORKSPACE_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)

RASTA_BIN=${RASTA_BIN:-/home/tstih/data/tstih/rasta/bin/rasta}
RASTA_WIDTH=${RASTA_WIDTH:-900}
RASTA_HEIGHT=${RASTA_HEIGHT:-900}
RASTA_BPP=${RASTA_BPP:-1}
RASTA_FB=${RASTA_FB:-/tmp/rasta.fb}
RASTA_PORT=${RASTA_PORT:-5000}
RASTA_SCALE=${RASTA_SCALE:-1}
RASTA_CURSOR=${RASTA_CURSOR:-off}
RASTA_INVERSE=${RASTA_INVERSE:-on}

if [[ ! -x "${RASTA_BIN}" ]]; then
    echo "missing Rasta binary: ${RASTA_BIN}" >&2
    exit 1
fi

EFFECTIVE_WIDTH=${RASTA_WIDTH}
if (( RASTA_BPP == 1 )) && (( RASTA_WIDTH % 8 != 0 )); then
    EFFECTIVE_WIDTH=$(( ((RASTA_WIDTH + 7) / 8) * 8 ))
fi

RASTA_PID=
cleanup() {
    local status=$?

    if [[ -n "${RASTA_PID}" ]] && kill -0 "${RASTA_PID}" 2>/dev/null; then
        kill "${RASTA_PID}" 2>/dev/null || true
        wait "${RASTA_PID}" 2>/dev/null || true
    fi
    exit "${status}"
}
trap cleanup EXIT INT TERM

# Rasta output must not enter the GDB/MI stream consumed by VS Code.
"${RASTA_BIN}" \
    --width "${EFFECTIVE_WIDTH}" \
    --height "${RASTA_HEIGHT}" \
    --bpp "${RASTA_BPP}" \
    --framebuffer "${RASTA_FB}" \
    --port "${RASTA_PORT}" \
    --scale "${RASTA_SCALE}" \
    --cursor "${RASTA_CURSOR}" \
    --inverse "${RASTA_INVERSE}" \
    >/dev/null 2>&1 &
RASTA_PID=$!

sleep 0.5
if [[ -e "${RASTA_FB}" ]]; then
    chmod 666 "${RASTA_FB}" 2>/dev/null || true
fi

docker run --rm -i \
    --network host \
    --cap-add SYS_PTRACE \
    --security-opt seccomp=unconfined \
    -u "$(id -u):$(id -g)" \
    -v "${WORKSPACE_DIR}:${WORKSPACE_DIR}" \
    -v /tmp:/tmp \
    -e GEM_RESOURCE_DIR="${GEM_RESOURCE_DIR:-/opt/gemix/share/gem}" \
    -e GEM_RASTA_CURSOR="${RASTA_CURSOR}" \
    -e GEM_RASTA_INVERSE="${RASTA_INVERSE}" \
    -e GEM_VDI_WIDTH="${EFFECTIVE_WIDTH}" \
    -e GEM_VDI_HEIGHT="${RASTA_HEIGHT}" \
    -e GEM_RASTA_FRAMEBUFFER="${RASTA_FB}" \
    -e GEM_RASTA_HOST=127.0.0.1 \
    -e GEM_RASTA_PORT="${RASTA_PORT}" \
    -e ASAN_OPTIONS=detect_leaks=0 \
    -w "${WORKSPACE_DIR}" \
    "${IMAGE}" bash -c '
        mode=$1; workspace=$2; shift 2
        if [[ $mode == 1 ]]; then
            exec bash "$workspace/scripts/gemix/gemd-session.sh" "$@"
        fi
        exec "$@"
    ' bash "${GEMD_MODE}" "${WORKSPACE_DIR}" "${DEBUGGER_COMMAND[@]}"
