FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# Elementary / EFL dev stack (Ubuntu)
# libefl-all-dev pulls in Elementary + core EFL libs.
# X11 + GL + Wayland headers included to cover common backends.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libefl-all-dev \
    libx11-dev libxext-dev libxrender-dev libxrandr-dev libxinerama-dev libxi-dev \
    libxkbcommon-dev \
    libgl1-mesa-dev libgles2-mesa-dev \
    libwayland-dev wayland-protocols \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work
