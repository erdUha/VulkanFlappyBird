#!/bin/bash
set -e

echo; echo "Building Project embedFiles..."; echo

cmake -S . -B ./build/linux
cmake --build ./build/linux --target embedFiles
