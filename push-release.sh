#!/bin/bash

set -e
WORKDIR=/tmp/ml-gridengine-executor/

# get the last release
git clone -b release "https://${GH_TOKEN}@github.com/haowen-xu/ml-gridengine-executor.git" "${WORKDIR}"

# deploy the main executable
cp build/ml-gridengine-executor "${WORKDIR}"

# deploy the tests
mkdir -p "${WORKDIR}/tests/"
cp build/ml-gridengine-executor-unit-tests "${WORKDIR}/tests/unit-tests"
cp -R tests/integrated-tests "${WORKDIR}/tests/"

# push to release branch
cd /tmp/ml-gridengine-executor/
git add --all .
if git commit -m "Travis CI Auto Builder"; then
    git push origin release
fi
