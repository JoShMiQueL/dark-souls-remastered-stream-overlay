# Dark Souls Tracker - Build Script (PowerShell)
# Uses MSBuild (Visual Studio's standard build system)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Dark Souls Tracker - Build Script" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Find MSBuild — try vswhere first, then fall back to known paths
$msbuildPath = $null

# vswhere ships with VS 2017+ and the Build Tools
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (Test-Path $vswhere) {
    $vsInstallPath = & $vswhere -latest -requires Microsoft.Component.MSBuild -property installationPath 2>$null
    if ($vsInstallPath) {
        $candidate = Join-Path $vsInstallPath "MSBuild\Current\Bin\MSBuild.exe"
        if (Test-Path $candidate) { $msbuildPath = $candidate }
    }
}

# Fall back to well-known paths (newest first)
if ($null -eq $msbuildPath) {
    $msbuildPaths = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2026\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2026\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2026\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
    )

    foreach ($path in $msbuildPaths) {
        if (Test-Path $path) {
            $msbuildPath = $path
            break
        }
    }
}

if ($null -eq $msbuildPath) {
    Write-Host "ERROR: MSBuild not found!" -ForegroundColor Red
    Write-Host "Please install Visual Studio 2019, 2022 or 2026 with C++ build tools" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Found MSBuild at:" -ForegroundColor Green
Write-Host "  $msbuildPath" -ForegroundColor Gray
Write-Host ""

# Change to script directory
$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $scriptPath
Set-Location ..

# Build configuration
$configuration = "Release"
$platform = "x64"

Write-Host "Building: $configuration|$platform" -ForegroundColor Yellow
Write-Host ""

# Build using MSBuild
$projectFile = "DarkSoulsTracker.vcxproj"
$buildArgs = @(
    $projectFile,
    "/p:Configuration=$configuration",
    "/p:Platform=$platform",
    "/p:OutDir=build\$configuration\",
    "/p:IntDir=build\$configuration\obj\",
    "/t:Build",
    "/v:minimal",
    "/nologo"
)

$result = & $msbuildPath $buildArgs

if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "========================================" -ForegroundColor Red
    Write-Host "BUILD FAILED" -ForegroundColor Red
    Write-Host "========================================" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Green
Write-Host "BUILD SUCCESSFUL" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Output files:" -ForegroundColor Cyan
if (Test-Path "build\$configuration\dinput8.dll") { Write-Host "  - build\$configuration\dinput8.dll" -ForegroundColor Gray }
if (Test-Path "build\$configuration\dinput8.lib") { Write-Host "  - build\$configuration\dinput8.lib" -ForegroundColor Gray }
if (Test-Path "build\$configuration\dinput8.exp") { Write-Host "  - build\$configuration\dinput8.exp" -ForegroundColor Gray }
Write-Host ""
Write-Host "To install, copy dinput8.dll to your Dark Souls Remastered folder:" -ForegroundColor Yellow
Write-Host "  build\$configuration\dinput8.dll" -ForegroundColor Gray
