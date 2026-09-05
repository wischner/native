#!/usr/bin/env bash
# Run Docker-built Vision in a dedicated black/white TWM desktop.
# Host resources and the user's TWM configuration are never modified.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
workspace_dir=$(cd "${script_dir}/../../.." && pwd)
nested_display=${NATIVE_X11_DISPLAY:-:10}
if [[ ! ${nested_display} =~ ^:[0-9]+$ ]]; then
    echo 'NATIVE_X11_DISPLAY must be a local display such as :10' >&2
    exit 2
fi
number=${nested_display#:}
if [[ -e /tmp/.X11-unix/X${number} || -e /tmp/.X${number}-lock ]]; then
    echo "Display ${nested_display} is already in use" >&2
    exit 1
fi
for command in Xephyr twm xrdb xdpyinfo xsetroot docker; do
    command -v "${command}" >/dev/null
done

Xephyr "${nested_display}" -ac -br -noreset -screen 1280x900 \
    -nolisten tcp -title "Native — TWM on Xephyr ${nested_display} (B&W)" &
server_pid=$!
manager_pid=
cleanup() {
    if [[ -n ${manager_pid} ]]; then kill "${manager_pid}" 2>/dev/null || true; fi
    kill "${server_pid}" 2>/dev/null || true
}
trap cleanup EXIT
trap 'exit 130' INT TERM
ready=false
for ((attempt = 0; attempt < 100; ++attempt)); do
    if DISPLAY="${nested_display}" xdpyinfo >/dev/null 2>&1; then
        ready=true
        break
    fi
    kill -0 "${server_pid}" 2>/dev/null || exit 1
    sleep 0.1
done
if [[ ${ready} != true ]]; then
    echo "Xephyr ${nested_display} did not become ready" >&2
    exit 1
fi
export DISPLAY=${nested_display}
xrdb -load "${script_dir}/xresources-bw"
xsetroot -gray
twm -f "${script_dir}/twmrc-bw" &
manager_pid=$!
"${workspace_dir}/scripts/linux/docker-debug.sh" \
    wischner/gcc-x86_64-linux-x11:latest \
    "${workspace_dir}/build/linux-x11/src/vision"
