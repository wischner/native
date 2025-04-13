# Widgets

Install neXtaw (Athena Widgets for Window Maker)

1. Prerequisites

   ```bash
   sudo apt install build-essential libxaw7-dev libxmu-dev libxpm-dev libxt-dev flex bison
   ```

2. Download neXtaw
   ```bash
   mkdir ~/src
   cd ~/src
   wget http://invisible-island.net/datafiles/release/neXtaw.tar.gz
   tar xf neXtaw.tar.gz
   cd neXtaw-\*
   ```
3. Compile and install

   ```bash
   CFLAGS="-fPIC" ./configure --prefix=/usr/local --enable-shared
   make
   sudo make install
   ```

4. Hack - compile shared library

   ```bash
   cd X11/neXtaw
   gcc -shared -Wl,-soname,libneXtaw.so.0 -o libneXtaw.so.0 *.o -lXaw -lXmu -lXt -lXpm -lX11

   ```

5. Test it.
   Try running xedit or xfontsel with neXtaw styling:
   ```bash
   LD_PRELOAD=/usr/local/lib/libneXtaw.so xmessage "Hello from neXtaw!"
   ```
   Should show scrollbars and buttons with a slick NeXTSTEP look.
