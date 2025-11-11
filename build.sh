#!/bin/bash
set -euxo

cd ./shaders
for vertFile in *.vert; do glslc $vertFile -o "$vertFile.spv"; done
for fragFile in *.frag; do glslc $fragFile -o "$fragFile.spv"; done
cd ../

cd ./lib/embedFiles
./buildEmbedFiles.sh
cd ../..
./lib/embedFiles/build/linux/embedFiles

# deleting compiled shaders bc not need them
cd ./shaders
for spvFile in *.spv; do rm -rf $spvFile; done
cd ../

echo; echo "Building Project VulkanFlappyBird..."; echo

cmake -S . -B build/linux
cmake --build build/linux --target VulkanFlappyBird

rm -rf ./build/linux/Release
mkdir ./build/linux/Release
cp -rf ./build/linux/VulkanFlappyBird ./build/linux/Release
cp -rf ./static ./build/linux/Release

echo "Done!"
echo; echo "Executing..."; echo

./build/linux/Release/VulkanFlappyBird

