#!/bin/bash

mkdir -p build || exit 1
sudo docker run -ti --rm -v "$(pwd)":"$(pwd)" -w "$(pwd)" \
    haowenxu/static-cpp-build \
    sh -c '
        cd build && \
        cmake \
            -DCMAKE_C_COMPILER=/usr/bin/x86_64-alpine-linux-musl-gcc \
            -DCMAKE_CXX_COMPILER=/usr/bin/x86_64-alpine-linux-musl-g++ \
            -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
            -DCMAKE_EXE_LINKER_FLAGS="-static" \
            -DBUILD_SHARED_LIBS=off \
            -DCMAKE_BUILD_TYPE=Release \
            .. && \
        make VERBOSE=1 && \
        strip -s ml-gridengine-executor && \
        strip -s ml-gridengine-executor-unit-tests
    '
