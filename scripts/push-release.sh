#!/bin/bash

WORKDIR=/tmp/ml-gridengine-executor/

# get the last release
git clone -b release "https://${GH_TOKEN}@github.com/haowen-xu/ml-gridengine-executor.git" "${WORKDIR}" || exit 1

# deploy the main executable
cp build/ml-gridengine-executor "${WORKDIR}" || exit 1

# deploy the tests
mkdir -p "${WORKDIR}/tests/" || exit 1
cp build/ml-gridengine-executor-unit-tests "${WORKDIR}/tests/unit-tests" || exit 1
cp -R tests/integrated-tests "${WORKDIR}/tests/" || exit 1

# push to release branch
cd /tmp/ml-gridengine-executor/ || exit 1
git add --all . || exit 1
if git commit -m "Travis CI Auto Builder"; then
    git push origin release
fi
