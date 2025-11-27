#!/bin/bash
set -e

cmake -B . -S .. \
    -DCMAKE_BUILD_TYPE=Debug #\
    #-DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1"

cmake --build .

cd ../test
../build/lumin --run ./test.lum
cd ../build