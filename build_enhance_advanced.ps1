# Enhanced Advanced Module Build Script
$ErrorActionPreference = "Stop"

# VS 2022 환경 설정
$vsPath = "D:\Program Files\Microsoft Visual Studio\2022\Professional"
$vcVarsPath = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"

# 빌드 디렉토리
$buildDir = "build\enhance_adv_standalone"
$moduleDir = "modules\enhance_advanced"

Write-Host "Setting up Visual Studio environment..."
cmd /c "`"$vcVarsPath`" && set" | ForEach-Object {
    if ($_ -match '^(.+?)=(.*)$') {
        [Environment]::SetEnvironmentVariable($matches[1], $matches[2])
    }
}

Write-Host "Creating build directory..."
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

Write-Host "Configuring CMake..."
$cmakeArgs = @(
    "-G", "Ninja"
    "-DCMAKE_BUILD_TYPE=Release"
    "-DBUILD_TESTS=ON"
    "-DCMAKE_PREFIX_PATH=D:/Program Files/Microsoft Visual Studio/2022/Professional/VC/vcpkg/installed/x64-windows"
    "-DCMAKE_TOOLCHAIN_FILE=D:/Program Files/Microsoft Visual Studio/2022/Professional/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
    "../../modules/enhance_advanced"
)

Push-Location $buildDir
try {
    & cmake $cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed with exit code $LASTEXITCODE"
    }

    Write-Host "Building xpe_enhance_advanced..."
    & cmake --build . --config Release --target xpe_enhance_advanced -j 8
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }

    Write-Host "Building test_enhance_advanced..."
    & cmake --build . --config Release --target test_enhance_advanced -j 8
    if ($LASTEXITCODE -ne 0) {
        throw "Test build failed with exit code $LASTEXITCODE"
    }

    Write-Host "Build completed successfully!"
    Write-Host "DLL location: $((Get-ChildItem -Recurse -Filter xpe_enhance_advanced.dll).FullName)"

} finally {
    Pop-Location
}
