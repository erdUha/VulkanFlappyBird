echo ""; echo "Building Project embedFiles..."; echo ""

cmake -G "Visual Studio 17 2022" -A x64 -S . -B .\build\windows
if ($LASTEXITCODE -ne 0) { exit 1 }

cmake --build .\build\windows --config Release --target embedFiles
if ($LASTEXITCODE -ne 0) { exit 1 }
