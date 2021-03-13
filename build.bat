@echo on
del CMakeCache.txt
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release .
ninja
