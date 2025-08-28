# syntax=docker/dockerfile:1
FROM ubuntu:24.04
ARG DEBIAN_FRONTEND=noninteractive
ARG UID=1000
ARG GID=1000
ARG TOOLKIT="X11"

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git pkg-config ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Toolkit-specific deps
# (X11 bundle includes pixman + xrandr + base X11 devs)
RUN set -eux; \
  case "$TOOLKIT" in \
    X11|OPENLOOK|MOTIF) \
      apt-get update && apt-get install -y --no-install-recommends \
        libx11-dev libxext-dev libxrender-dev libxrandr-dev \
        libxfixes-dev libxcursor-dev libxinerama-dev \
        libpixman-1-dev \
        && rm -rf /var/lib/apt/lists/* ;; \
    SDL2) \
      apt-get update && apt-get install -y --no-install-recommends \
        libsdl2-dev \
        && rm -rf /var/lib/apt/lists/* ;; \
    GNUSTEP) \
      apt-get update && apt-get install -y --no-install-recommends \
        gnustep-devel \
        && rm -rf /var/lib/apt/lists/* ;; \
    ELEMENTARY) \
      apt-get update && apt-get install -y --no-install-recommends \
        libelementary-dev \
        && rm -rf /var/lib/apt/lists/* || true ;; \
    *) echo "Unknown TOOLKIT=$TOOLKIT"; exit 1;; \
  esac

RUN groupadd -g ${GID} builder && useradd -m -u ${UID} -g ${GID} builder
USER builder
WORKDIR /work
