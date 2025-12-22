#!/bin/bash

git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp

# Remove Metal flags for Linux build
cmake -B build -DCMAKE_BUILD_TYPE=Release -DLLAMA_METAL=OFF

# Get number of CPU cores for Linux
CORES=$(nproc --all)
cmake --build build --config Release -- -j$CORES

# Copy the built server to your project directory
cp ./build/bin/server ../server