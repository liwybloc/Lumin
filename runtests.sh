#!/bin/bash
set -e
cmake .. 
cmake --build .
cd ../test
../build/lumin --run ./test.lum
cd ../build
