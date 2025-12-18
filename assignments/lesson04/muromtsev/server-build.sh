#! /bin/bash

git clone https://github.com/ggerganov/llama.cpp
cd llama.cpp
cmake -B build -DCMAKE_BUILD_TYPE=Release -DGGML_METAL=ON -DCMAKE_OSX_ARCHITECTURES=arm64
cmake --build build --config Release -- -j$(sysctl -n hw.ncpu)