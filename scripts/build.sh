#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p build
cd build
cmake ..
cmake --build .
echo "Build complete: build/sandboxx"
