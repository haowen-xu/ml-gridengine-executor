#!/usr/bin/env bash

cd ./build/ || exit 1

# run unit tests
export SIGNAL_HANDLER_EXAMPLE_EXE="$(pwd)/SignalHandlerExample"
./ml-gridengine-executor-unit-tests --rng-seed=time || exit 1

# run integrated tests
[[ -x integrated-tests ]] || ln -sf ../tests/integrated-tests/ integrated-tests || exit 1
python -m pytest integrated-tests
