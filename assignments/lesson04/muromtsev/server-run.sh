#! /bin/bash

./llama.cpp/build/bin/llama-server \
  -m llama.cpp/models/Qwen2.5-0.5B-Instruct-Q4_K_M.gguf \
  -c 4096 -ngl 999 \
  --host 127.0.0.1 --port 8080 \
  --keep -1 