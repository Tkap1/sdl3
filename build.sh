#!/bin/bash -e

WARN="-Wall -Werror -Wno-unused-but-set-variable -Wno-unused-variable -Wshadow -Wno-write-strings -Wno-unused-function -Wno-missing-braces"
CXXFLAGS="-std=c++20 -g $WARN -I./src"
LIBS="-lSDL3 -lshaderc_shared"

g++ src/main.cpp $CXXFLAGS -o main $LIBS
