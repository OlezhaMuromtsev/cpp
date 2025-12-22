#! /bin/bash

./llama.cpp/build/bin/llama-server \
  -m $1 \
  -c 4096 -ngl 999 \
  --host 127.0.0.1 --port 8080 \
  --keep -1 