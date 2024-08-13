@echo off
setlocal

echo -------------------------------------
echo Building with CMake

mkdir -p build
cd build
cmake .. -DBUILD_SHARED_LIBS=off
cmake --build .

echo -------------------------------------