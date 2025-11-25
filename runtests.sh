#!/bin/bash
set -e
cmake .. 
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
cd ../natives
../build/lumin --run ./standard.lum
cd ../build
