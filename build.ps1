# building shaders
glslc.exe .\shaders\objectShader.vert -o .\shaders\objectShader.vert.spv
if ($LASTEXITCODE -ne 0) { exit 1 }
glslc.exe .\shaders\objectShader.frag -o .\shaders\objectShader.frag.spv
if ($LASTEXITCODE -ne 0) { exit 1 }
glslc.exe .\shaders\shadowMapShader.vert -o .\shaders\shadowMapShader.vert.spv
if ($LASTEXITCODE -ne 0) { exit 1 }
glslc.exe .\shaders\shadowMapShader.frag -o .\shaders\shadowMapShader.frag.spv
if ($LASTEXITCODE -ne 0) { exit 1 }

cd .\lib\embedFiles
.\buildEmbedFiles.ps1
cd ..\..
.\lib\embedFiles\build\windows\Release\embedFiles.exe

rm -r -fo .\shaders\objectShader.vert.spv
rm -r -fo .\shaders\objectShader.frag.spv
rm -r -fo .\shaders\shadowMapShader.vert.spv
rm -r -fo .\shaders\shadowMapShader.frag.spv

echo ""; echo "Building Project VulkanFlappyBird..."; echo ""

cmake -DCMAKE_CXX_FLAGS="/EHsc" -G "Visual Studio 17 2022" -A x64 -S . -B .\build\windows
if ($LASTEXITCODE -ne 0) { exit 1 }

cmake --build .\build\windows --config Release --target VulkanFlappyBird
if ($LASTEXITCODE -ne 0) { exit 1 }

xcopy .\static .\build\windows\Release\static /s /i /q /y

echo "Done!"
echo ""; echo "Executing..."; echo ""

.\build\windows\Release\VulkanFlappyBird.exe
