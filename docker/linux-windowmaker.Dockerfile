FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# Window Maker dev toolkits + X11 stack
# Debian/Ubuntu package names shown; adjust if your distro differs.
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Window Maker toolkits
    libwings-dev \
    libwraster-dev \
    # common X11 + friends (similar to linux-x11 variant)
    libx11-dev \
    libxext-dev \
    libxrender-dev \
    libxrandr-dev \
    libxfixes-dev \
    libxcursor-dev \
    libxinerama-dev \
    libpixman-1-dev \
    # image codecs often used by wraster
    libjpeg-dev \
    libpng-dev \
    libtiff-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
