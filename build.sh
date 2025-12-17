#!/bin/bash

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Install (requires sudo)
echo "Installing Sigma compiler..."
sudo make install

echo "✅ Sigma installed! Use 'sig filename.sgm' to run programs"