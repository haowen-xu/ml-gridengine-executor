#!/usr/bin/env bash

set -e
cd ./build/

# run unit tests
./ml-gridengine-executor-unit-tests

# run integrated tests
[[ -x integrated-tests ]] || ln -sf ../tests/integrated-tests/ integrated-tests
python -m pytest integrated-tests
