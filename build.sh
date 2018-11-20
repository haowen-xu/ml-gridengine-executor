#!/bin/bash

sudo docker run -ti --rm -v "$(pwd)":"$(pwd)" -w "$(pwd)" \
    haowenxu/static-cpp-build \
    sh -c '
    rm -rf build && \
        mkdir -p build && \
        cd build && \
        cmake \
            -DCMAKE_FIND_LIBRARY_SUFFIXES=".a" \
            -DBUILD_SHARED_LIBS=off \
            -DCMAKE_EXE_LINKER_FLAGS="-static" \
            .. && \
        make VERBOSE=1 && \
        cp ml_gridengine_executor ../ml-gridengine-executor && \
        cd .. && \
        strip -s ml-gridengine-executor
    '
