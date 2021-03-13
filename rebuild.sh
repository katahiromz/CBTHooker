#!/usr/bin/env bash
rm CMakeCache.txt
cmake -G "MSYS Makefiles" .
make -j3
