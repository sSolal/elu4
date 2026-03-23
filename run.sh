#!/bin/bash

# Build
echo "Building..."
mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cd ..

# Run
echo "Running..."
./build/game
