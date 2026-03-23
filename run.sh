#!/bin/bash

# Install dependencies
echo "Installing dependencies..."
sudo apt-get update
sudo apt-get install -y libglfw3-dev libglew-dev cmake g++ libglm-dev

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
