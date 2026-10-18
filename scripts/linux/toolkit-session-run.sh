#!/usr/bin/env bash
set -euo pipefail

# Run a program inside the nested window-manager session its toolkit
# expects.
#
# Motif/CDE, OPEN LOOK and Window Maker need matching desktops. Their
# windows are managed by dtwm, olwm, or Window Maker, which own the frames,
# the resize behaviour, and the decorations those toolkits draw
# against. Started on an ordinary desktop they come up on whatever
# window manager is already running, and the result is not the toolkit
# these backends target. Each image ships a helper that raises Xephyr,
# starts the matching window manager inside it, and runs a command
# there; this script selects the right one and hands the program over.

usage() {
    echo "usage: $0 <openmotif|openlook|wmaker> <app-path> [app-args...]" >&2
    exit 2
}

# The image helpers have historical fixed display defaults. Select
# isolated displays so multiple local runs and smoke tests can coexist
# without fighting over the same X sockets.
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

if [[ $# -lt 2 ]]; then
    usage
fi

TOOLKIT=$1
APP_PATH=$2
shift 2

case "$TOOLKIT" in
openmotif | motif | cde)
    IMAGE=${OPENMOTIF_IMAGE:-wischner/gcc-x86_64-linux-motif:latest}
    SESSION=cde-xephyr
    DISPLAY_VARIABLE=CDE_DISPLAY
    PARENT_DISPLAY_VARIABLE=CDE_PARENT_DISPLAY
    ;;
openlook)
    IMAGE=${OPENLOOK_IMAGE:-wischner/gcc-x86_64-linux-openlook:latest}
    SESSION=openlook-xephyr
    DISPLAY_VARIABLE=OPENLOOK_DISPLAY
    PARENT_DISPLAY_VARIABLE=OPENLOOK_PARENT_DISPLAY
    ;;
wmaker | window-maker)
    IMAGE=${WMAKER_IMAGE:-wischner/gcc-x86_64-linux-window-maker:latest}
    SESSION=window-maker-xephyr
    DISPLAY_VARIABLE=WINDOW_MAKER_DISPLAY
    PARENT_DISPLAY_VARIABLE=WINDOW_MAKER_PARENT_DISPLAY
    ;;
*)
    usage
    ;;
esac

if [[ ! -x "$APP_PATH" ]]; then
    echo "missing app binary: $APP_PATH" >&2
    exit 1
fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
WORKSPACE_DIR=$(cd "${SCRIPT_DIR}/../.." && pwd)

DOCKER_ARGS=(
    run --rm
    -v "${WORKSPACE_DIR}:${WORKSPACE_DIR}"
    -v /tmp/.X11-unix:/tmp/.X11-unix
    -w "${WORKSPACE_DIR}"
    # Debug builds carry the leak sanitizer, and these toolkits leak
    # inside their own libraries and fontconfig at exit. Those reports
    # are not this project's and they turn every clean run into a
    # failure, so leak checking stays off for a normal run.
    -e ASAN_OPTIONS=detect_leaks=0
    -e "${DISPLAY_VARIABLE}=$(find_free_display 100 199)"
    -e "${PARENT_DISPLAY_VARIABLE}=$(find_free_display 200 299)"
)

# CDE needs container root for its RPC services, on an isolated network.
if [[ "$SESSION" != cde-xephyr ]]; then
    DOCKER_ARGS+=(--network host -u "$(id -u):$(id -g)")
fi

# Xephyr is a nested server, so it needs a parent display to open on.
# Without one the image's helper falls back to a headless Xvfb and the
# session runs where nobody can see it.
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

exec docker "${DOCKER_ARGS[@]}" "${IMAGE}" \
    "${SESSION}" "${APP_PATH}" "$@"
