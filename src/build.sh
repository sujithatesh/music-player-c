#!/bin/bash

# Detect the operating system
OS=$(uname)

if [[ "$OS" == "Linux" ]]; then
    echo "Building for Linux..."
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/sujith/gitshit/raylib-5.0_linux_amd64/lib
    gcc player.c -Wall -g -o player -lasound\
    -L /home/sujith/gitshit/real-raylib/build/raylib/lib \
    -I /home/sujith/gitshit/real-raylib/build/raylib/include \
-I/usr/include/gtk-3.0 -I/usr/include/pango-1.0 -I/usr/include/cloudproviders -I/usr/include/cairo -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/at-spi2-atk/2.0 -I/usr/include/at-spi-2.0 -I/usr/include/atk-1.0 -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include -I/usr/include/fribidi -I/usr/include/pixman-1 -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/gio-unix-2.0 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/sysprof-6 -pthread -lgtk-3 -lgdk-3 -lz -lpangocairo-1.0 -lcairo-gobject -lgdk_pixbuf-2.0 -latk-1.0 -lpango-1.0 -lcairo -lharfbuzz -lgio-2.0 -lgobject-2.0 -lglib-2.0 \
    -lraylib -lm \

elif [[ "$OS" == "Darwin" ]]; then
    echo "Building for macOS..."
    gcc -v -Wall -Wextra -g player.c -o player \
    -L /Users/sujith.varkala/C/raylib/raylib-5.5_macos/lib \
    -I /Users/sujith.varkala/C/raylib/raylib-5.5_macos/include \
    -lraylib -lm \
    && export DYLD_LIBRARY_PATH=/Users/sujith.varkala/C/raylib/raylib-5.5_macos/lib:$DYLD_LIBRARY_PATH \
    && ./player

else
    echo "Unsupported OS: $OS"
    exit 1
fi
