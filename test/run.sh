#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build
"${CXX:-g++}" -std=c++11 -Wall -Wextra -Wconversion -Werror \
  -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Isrc src/ZeroAllocJSON.cpp test/test.cpp -o build/zeroallocjson_tests
./build/zeroallocjson_tests
