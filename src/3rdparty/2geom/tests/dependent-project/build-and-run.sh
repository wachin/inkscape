#!/bin/bash

set -x
mkdir -p ../../opt2
mkdir -p build-as-subproject
(
    cd build-as-subproject;
    cmake ..  -D2GEOM_AS_SUBPROJECT=ON \
              -DCMAKE_INSTALL_PREFIX=../../../opt2
    make main -j 2
    ./main
    make install
)

mkdir -p build-with-find-package
(
    cd build-with-find-package;
    cmake .. -D2GEOM_AS_SUBPROJECT=OFF \
             -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" \
             -DCMAKE_INSTALL_PREFIX=../../../opt2 \
             -D2Geom_DIR="$PWD/../../../opt/lib/cmake/2Geom"
    make main -j 2
    ./main
    make install
)
