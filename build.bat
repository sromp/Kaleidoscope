@echo off
IF NOT EXIST build mkdir build
pushd "build"
    cmake -S "../code" 
    cmake --build .
popd
