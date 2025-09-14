#!/bin/bash

# Detect the operating system
OS=$(uname)

if [[ "$OS" == "Linux" ]]; then
    echo "Building for Linux..."
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/home/sujith/gitshit/raylib-5.0_linux_amd64/lib
    clang player.c -finstrument-functions -pedantic -Wall -Wpedantic -g -o ../player -lasound\
        -I ../include\
        -L../lib\
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
