#!/bin/bash

set -e
git clone -b release "https://${GH_TOKEN}@github.com/haowen-xu/ml-gridengine-executor.git" /tmp/ml-gridengine-executor
cp build/ml-gridengine-executor /tmp/ml-gridengine-executor/
cd /tmp/ml-gridengine-executor/
git add --all .
if git commit -m "Travis CI Auto Builder"; then
    git push origin release
fi
