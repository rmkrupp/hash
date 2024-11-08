#!/bin/bash

function run() {
    ./configure.py $@ && ninja --quiet || exit 1
    echo "$(/usr/bin/time --format="%E" ./reuse_test 2>&1| tr -d '\n') $@"
}

echo "benchmarking $(git describe --dirty --always)" >&2

# debug build is very slow, by default don't do it
run 
run --build=release
run --build=release --build-native=mtune
run --build=release --build-native=march
run --build=release --build-native=both
run --build=release --o3
run --build=release --o3 --build-native=mtune
run --build=release --o3 --build-native=march
run --build=release --o3 --build-native=both

