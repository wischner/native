FROM ubuntu:24.04@sha256:68ee22004a9109b44973313b02ff7b1de250f1de0f5ecb628d73f579aaff33ac
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake pkg-config gdb libgtk-4-dev libgranite-7-dev gsettings-desktop-schemas \
    libgtksourceview-5-dev libpng-dev libjpeg-dev libfontconfig1-dev \
    libegl1-mesa-dev libgl1-mesa-dri weston dbus-daemon fonts-dejavu-core \
    elementary-icon-theme ca-certificates git meson ninja-build sassc gettext && rm -rf /var/lib/apt/lists/*

# elementary OS 8's GTK 4.14-era stylesheet. Keep its GPL notices in the image.
RUN git init /opt/elementary-stylesheet && \
    git -C /opt/elementary-stylesheet fetch --depth 1 \
        https://github.com/elementary/stylesheet.git \
        9c713d0f4c35f80a783e5fa6845a39d977c7140c && \
    git -C /opt/elementary-stylesheet checkout --detach FETCH_HEAD && \
    meson setup /opt/elementary-stylesheet/build /opt/elementary-stylesheet --prefix=/usr && \
    ninja -C /opt/elementary-stylesheet/build install
