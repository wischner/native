FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# X11 + pixman + xrandr headers/libs for linux-x11 builds
RUN apt-get update && apt-get install -y --no-install-recommends \
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxrandr-dev \
    libxfixes-dev \
    libxcursor-dev \
    libxinerama-dev \
    libpixman-1-dev \
    gdb \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
