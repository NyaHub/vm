#!/bin/bash
mkdir -p build &&\
cd build &&\
cmake -DCMAKE_BUILD_TYPE=Debug .. &&\
make &&\
# clear &&\
./blockchain 2048.obj&&\
cd ../