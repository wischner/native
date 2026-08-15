#!/usr/bin/env bash
set -euo pipefail

# Run the debugger supplied by VS Code inside the selected Linux image.
if [[ $# -lt 2 ]]; then
    echo "usage: $0 <docker-image> <debugger> [debugger-args...]" >&2
    exit 2
fi

IMAGE=$1
shift

# cppdbg may append its MI option to debuggerPath and pass the resulting
# command as one argument. Docker needs the executable and its options as
# separate arguments.
DEBUGGER_COMMAND=("$@")
if (( ${#DEBUGGER_COMMAND[@]} == 1 )); then
    read -r -a DEBUGGER_COMMAND <<< "${DEBUGGER_COMMAND[0]}"
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
WORKSPACE_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)
DOCKER_ARGS=(
    run --rm -i
    --network host
    --cap-add SYS_PTRACE
    --security-opt seccomp=unconfined
    -v "${WORKSPACE_DIR}:${WORKSPACE_DIR}"
    -v /tmp/.X11-unix:/tmp/.X11-unix
    -w "${WORKSPACE_DIR}"
)

# Pass the active graphical session to programs launched by GDB.
if [[ -n "${DISPLAY:-}" ]]; then
    DOCKER_ARGS+=(-e "DISPLAY=${DISPLAY}")
fi

XAUTH_FILE=${XAUTHORITY:-${HOME}/.Xauthority}
if [[ -f "${XAUTH_FILE}" ]]; then
    DOCKER_ARGS+=(
        -e XAUTHORITY=/tmp/.Xauthority
        -v "${XAUTH_FILE}:/tmp/.Xauthority:ro"
    )
fi

exec docker "${DOCKER_ARGS[@]}" "${IMAGE}" "${DEBUGGER_COMMAND[@]}"
