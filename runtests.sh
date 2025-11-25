#!/bin/bash
set -e
cmake .. 
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
cd ../test
../build/lumin --run ./test.lum
cd ../build
