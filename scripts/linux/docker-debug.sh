#!/usr/bin/env bash
set -euo pipefail

# Run the debugger supplied by VS Code inside the selected Linux image.
#
# A backend whose toolkit needs its own window manager passes
# "--session <command>" ahead of the image. That command wraps the
# debugger inside the container, so the debugged program starts in the
# session its toolkit expects instead of the host desktop.
usage() {
    echo "usage: $0 [--session <command>] <docker-image>" \
        "<debugger> [debugger-args...]" >&2
    exit 2
}

SESSION_COMMAND=()
while [[ $# -gt 0 ]]; do
    case "$1" in
    --session)
        [[ $# -ge 2 ]] || usage
        SESSION_COMMAND=("$2")
        shift 2
        ;;
    *)
        break
        ;;
    esac
done

if [[ $# -lt 2 ]]; then
    usage
fi

IMAGE=$1
shift

# Xephyr and Xvfb create sockets in the host directory mounted below.
# The image helpers default to fixed displays (:2 and :99), so a normal
# run left open alongside F5 would make the debugger's server fail to
# start and its window manager fall through to the wrong display.
find_free_display() {
    local first=$1
    local last=$2
    local number

    for ((number = first; number <= last; ++number)); do
        if [[ ! -e "/tmp/.X11-unix/X${number}" &&
              ! -e "/tmp/.X${number}-lock" ]]; then
            printf ':%d' "${number}"
            return 0
        fi
    done

    echo "no free X display between :${first} and :${last}" >&2
    return 1
}

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

# Give each toolkit session displays distinct from the helpers' fixed
# defaults. The parent display is only used when the host DISPLAY is
# absent or cannot be reached from the container.
case "${SESSION_COMMAND[0]:-}" in
openlook-xephyr)
    DOCKER_ARGS+=(
        -e "OPENLOOK_DISPLAY=$(find_free_display 100 199)"
        -e "OPENLOOK_PARENT_DISPLAY=$(find_free_display 200 299)"
    )
    ;;
window-maker-xephyr)
    DOCKER_ARGS+=(
        -e "WINDOW_MAKER_DISPLAY=$(find_free_display 100 199)"
        -e "WINDOW_MAKER_PARENT_DISPLAY=$(find_free_display 200 299)"
    )
    ;;
esac

exec docker "${DOCKER_ARGS[@]}" "${IMAGE}" \
    "${SESSION_COMMAND[@]}" "${DEBUGGER_COMMAND[@]}"
