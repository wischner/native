#!/usr/bin/env bash
# Run Docker-built Vision in a dedicated black/white TWM desktop.
# Optional arguments run a debugger in the same session for cppdbg's pipe.
# Host resources and the user's TWM configuration are never modified.
#
# Xephyr, TWM, the X utilities and GDB all come from the X11 image; the
# host only needs Docker and a display for the nested server's window.
# On the host this script hands itself to docker-debug.sh as the session
# command and runs again inside the container, where it raises the
# session.
#
# The session follows the host desktop scale (Xft.dpi / 96, quarter
# steps). When the image provides Xwayland 23.1+ and the host runs
# Wayland, a rootful Xwayland at 1280x900 is scaled by the compositor, like
# any unscaled program. Otherwise Xephyr gets a scaled screen and the
# nested Xft.dpi carries the host value, so the X11 backend scales
# itself; TWM's font, spacing and root stipple are scaled to match while
# borders stay thin.
set -euo pipefail

script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
workspace_dir=$(cd "${script_dir}/../../.." && pwd)
image=${NATIVE_X11_IMAGE:-wischner/gcc-x86_64-linux-x11:latest}

if [[ ! -f /.dockerenv ]]; then
    command -v docker >/dev/null || {
        echo 'Missing host command: docker' >&2
        exit 1
    }
    if [[ $# == 0 ]]; then
        set -- "${workspace_dir}/build/linux-x11/src/vision"
    fi
    exec "${workspace_dir}/scripts/linux/docker-debug.sh" \
        --session "${script_dir}/run-twm.sh" "${image}" "$@"
fi

nested_display=${NATIVE_X11_DISPLAY:-}
if [[ -z ${nested_display} ]]; then
    for ((candidate = 10; candidate <= 99; ++candidate)); do
        if [[ ! -e /tmp/.X11-unix/X${candidate} && ! -e /tmp/.X${candidate}-lock ]]; then
            nested_display=:${candidate}
            break
        fi
    done
    if [[ -z ${nested_display} ]]; then
        echo 'No free nested X display between :10 and :99' >&2
        exit 1
    fi
fi
if [[ ! ${nested_display} =~ ^:[0-9]+$ ]]; then
    echo 'NATIVE_X11_DISPLAY must be a local display such as :10' >&2
    exit 2
fi
number=${nested_display#:}
if [[ -e /tmp/.X11-unix/X${number} || -e /tmp/.X${number}-lock ]]; then
    echo "Display ${nested_display} is already in use" >&2
    exit 1
fi
missing=()
for command in Xephyr twm xrdb xdpyinfo xwininfo xsetroot; do
    command -v "${command}" >/dev/null || missing+=("${command}")
done
if (( ${#missing[@]} )); then
    echo "Missing in ${image}: ${missing[*]}" >&2
    exit 1
fi

# The host desktop publishes its scale for X clients as Xft.dpi.
dpi=${NATIVE_X11_DPI:-}
if [[ -z ${dpi} && -n ${DISPLAY:-} ]]; then
    dpi=$(xrdb -query 2>/dev/null | awk '$1 == "Xft.dpi:" { print $2; exit }')
fi
if [[ ! ${dpi} =~ ^[0-9]+([.][0-9]+)?$ ]]; then
    dpi=96
fi
# Quarter steps keep near-96 values at 1, as the X11 backend does.
scale=$(awk -v dpi="${dpi}" 'BEGIN {
    s = int(dpi / 96 * 4 + 0.5) / 4
    if (s < 1) s = 1
    if (s > 4) s = 4
    print s
}')
scaled() {
    awk -v value="$1" -v scale="${scale}" \
        'BEGIN { printf "%d", int(value * scale + 0.5) }'
}

session_dir=$(mktemp -d)
width=1280
height=900
server=Xephyr
# Rootful window geometry needs Xwayland 23.1 or later.
if [[ ${scale} != 1 && -n ${WAYLAND_DISPLAY:-} &&
      -S ${XDG_RUNTIME_DIR:-/nonexistent}/${WAYLAND_DISPLAY} ]] &&
    command -v Xwayland >/dev/null &&
    Xwayland -help 2>&1 | grep -q -- '^-geometry'; then
    server=Xwayland
fi
if [[ ${server} == Xwayland ]]; then
    # The compositor scales the whole unscaled desktop.
    nested_dpi=96
    scale=1
    Xwayland "${nested_display}" -geometry "${width}x${height}" -decorate \
        -noreset -nolisten tcp >&2 &
else
    nested_dpi=${dpi}
    Xephyr "${nested_display}" -ac -br -noreset \
        -screen "$(scaled "${width}")x$(scaled "${height}")" -nolisten tcp \
        -title "Native - TWM on Xephyr ${nested_display} (B&W)" >&2 &
fi
server_pid=$!
manager_pid=
child_pid=
cleanup() {
    if [[ -n ${child_pid} ]]; then
        kill "${child_pid}" 2>/dev/null || true
        wait "${child_pid}" 2>/dev/null || true
    fi
    if [[ -n ${manager_pid} ]]; then
        kill "${manager_pid}" 2>/dev/null || true
        wait "${manager_pid}" 2>/dev/null || true
    fi
    kill "${server_pid}" 2>/dev/null || true
    wait "${server_pid}" 2>/dev/null || true
    rm -rf "${session_dir}"
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
    echo "${server} ${nested_display} did not become ready" >&2
    exit 1
fi
export DISPLAY=${nested_display}

# Scale TWM's spacing, not its strokes: border widths keep their
# configured device pixels. "fixed" (misc-fixed 6x13) is replaced by the
# installed bitmap face nearest 13 pixels times the scale, so text is a
# hand-tuned size rather than magnified glyphs.
fixed_font=fixed
if [[ ${scale} != 1 ]]; then
    target=$(scaled 13)
    fixed_font=$(
        for family in b\&h-lucidatypewriter xos4-terminus misc-fixed; do
            xlsfonts -fn "-${family}-medium-r-normal-*-*-*-*-*-*-*-iso8859-1" \
                2>/dev/null
        done | awk -F- -v target="${target}" '
            $8 > 0 {
                distance = $8 - target
                if (distance < 0) distance = -distance
                if (best == "" || distance < best_distance) {
                    best = $0
                    best_distance = distance
                }
            }
            END { print (best == "" ? "fixed" : best) }'
    )
fi
awk -v scale="${scale}" -v font="${fixed_font}" '
    $1 ~ /Font$/ && $2 == "\"fixed\"" { print $1 " \"" font "\""; next }
    $1 == "MoveDelta" && $2 ~ /^[0-9]+$/ {
        print $1 " " int($2 * scale + 0.5); next
    }
    { print }
' "${script_dir}/twmrc-bw" >"${session_dir}/twmrc"
echo "IconManagerGeometry \"=$(scaled 200)x$(scaled 5)+0+0\"" \
    >>"${session_dir}/twmrc"
{
    cat "${script_dir}/xresources-bw"
    echo "Xft.dpi: ${nested_dpi}"
} >"${session_dir}/xresources"

# The classic 50% stipple, each dot one scaled pixel.
cell=$(scaled 1)
root_pixmap=${session_dir}/gray.xbm
awk -v cell="${cell}" 'BEGIN {
    size = cell * 2
    bytes = int((size + 7) / 8)
    printf "#define gray_width %d\n#define gray_height %d\n", size, size
    printf "static unsigned char gray_bits[] = {"
    count = 0
    for (y = 0; y < size; ++y) {
        for (b = 0; b < bytes; ++b) {
            value = 0
            for (bit = 0; bit < 8; ++bit) {
                x = b * 8 + bit
                if (x < size && (int(x / cell) + int(y / cell)) % 2 == 0)
                    value += 2 ^ bit
            }
            printf "%s0x%02x", (count++ ? ", " : "\n   "), value
        }
    }
    print "};"
}' >"${root_pixmap}"

xrdb -load "${session_dir}/xresources" >&2
xsetroot -bitmap "${root_pixmap}" -fg black -bg white >&2
twm -f "${session_dir}/twmrc" >&2 &
manager_pid=$!
# The supplied configuration shows this window once TWM owns the display.
# Wait for it before allowing GDB or the application to create any windows.
ready=false
for ((attempt = 0; attempt < 100; ++attempt)); do
    if xwininfo -root -tree 2>/dev/null | grep -q '"TWM Icon Manager"'; then
        ready=true
        break
    fi
    kill -0 "${manager_pid}" 2>/dev/null || exit 1
    sleep 0.1
done
if [[ ${ready} != true ]]; then
    echo "TWM on ${nested_display} did not become ready" >&2
    exit 1
fi
# Only the debugger writes stdout: cppdbg uses it as the GDB/MI channel.
# It runs as a waited child, not in the foreground, because bash defers
# traps until a foreground command ends; a stop signal must still tear
# the session down. Explicit stdin keeps the MI pipe instead of the
# /dev/null background jobs otherwise receive.
"$@" <&0 &
child_pid=$!
status=0
wait "${child_pid}" || status=$?
child_pid=
exit "${status}"
