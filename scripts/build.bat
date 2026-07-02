@echo off
:: Configure, build and run tests with Ninja.
:: Usage: build.bat [Debug|Release]
setlocal enabledelayedexpansion

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Debug"

if /I not "%CONFIG%"=="Debug" if /I not "%CONFIG%"=="Release" (
    echo Usage: %~nx0 [Debug^|Release]
    exit /b 1
)

set "ROOT_DIR=%~dp0.."
set "BUILD_DIR=%ROOT_DIR%\build\%CONFIG%"

cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1

cmake --build "%BUILD_DIR%"
if errorlevel 1 exit /b 1

ctest --test-dir "%BUILD_DIR%" --output-on-failure -C %CONFIG%
exit /b %errorlevel%
