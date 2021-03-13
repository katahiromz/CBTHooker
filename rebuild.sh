#!/usr/bin/env bash
rm CMakeCache.txt
cmake -DCMAKE_BUILD_TYPE=Release -G "MSYS Makefiles" .
make -j3
