FROM native-build:base
ARG DEBIAN_FRONTEND=noninteractive

# Legacy X11 toolchain bits commonly needed by XView/OLIT builds
# We include imake/makedepend + a broad set of X11 dev libs.
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build tools frequently required by old trees
    xutils-dev \
    flex bison patch autoconf automake libtool \
    # Core X11 headers/libs
    libx11-dev libxext-dev libxt-dev libxmu-dev libxpm-dev libxaw7-dev \
    libice-dev libsm-dev \
    # Useful X11 add-ons used by some samples
    libxrandr-dev libxrender-dev libxkbfile-dev \
    # Common image libs sometimes needed by XView/Wraster-like code
    libjpeg-dev libpng-dev libtiff-dev \
    # You already use pixman/xrandr elsewhere
    libpixman-1-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /work

# Notes:
# - Many public XView/OLIT trees still use imake; the packages above cover that.
# - Keep the actual XView/OLIT sources in your repo (e.g., third_party/xview/)
#   or as a submodule that your inner build step fetches. This avoids relying on
#   fragile external URLs during the docker build itself.
