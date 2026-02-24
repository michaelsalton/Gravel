#!/usr/bin/env pwsh
# Build script for Gravel (Windows equivalent of Makefile)

param(
    [Parameter(Position=0)]
    [ValidateSet('build', 'run', 'configure', 'clean', 'all')]
    [string]$Target = 'all'
)

$BUILD_DIR = "build"
$BINARY = "$BUILD_DIR\bin\Debug\Gravel.exe"

function Build {
    Write-Host "Building project..." -ForegroundColor Cyan
    cmake --build $BUILD_DIR
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Build failed!" -ForegroundColor Red
        exit 1
    }
}

function Run {
    if (-not (Test-Path $BINARY)) {
        Write-Host "Binary not found. Building first..." -ForegroundColor Yellow
        Build
    }
    Write-Host "Running Gravel..." -ForegroundColor Green
    & $BINARY
}

function Configure {
    Write-Host "Configuring project..." -ForegroundColor Cyan
    cmake -S . -B $BUILD_DIR -DCMAKE_BUILD_TYPE=Release
}

function Clean {
    Write-Host "Cleaning build..." -ForegroundColor Cyan
    cmake --build $BUILD_DIR --target clean
}

# Main execution
switch ($Target) {
    'build'     { Build }
    'run'       { Run }
    'configure' { Configure }
    'clean'     { Clean }
    'all'       { Build; Run }
    default     { Build; Run }
}
