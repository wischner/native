FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# OpenMotif + the same X11 stack used by x11, plus pixman/xrandr
RUN apt-get update && apt-get install -y --no-install-recommends \
    libmotif-dev \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxrandr-dev \
    libxfixes-dev \
    libxcursor-dev \
    libxinerama-dev \
    libpixman-1-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
