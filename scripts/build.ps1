<#
.SYNOPSIS
    Configure, build and run tests with Ninja.
.PARAMETER Config
    Build configuration: Debug or Release.
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug"
)

$ErrorActionPreference = "Stop"

$RootDir = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $RootDir "build\$Config"

cmake -S $RootDir -B $BuildDir -G Ninja -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

cmake --build $BuildDir
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

ctest --test-dir $BuildDir --output-on-failure -C $Config
exit $LASTEXITCODE
