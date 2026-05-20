@echo off
setlocal

set "ROOT=%~dp0"
REM NDK toolchain. Override via env var NDK_HOME, or edit fallback below.
if defined NDK_HOME (
    set "NDK=%NDK_HOME%\toolchains\llvm\prebuilt\windows-x86_64"
) else (
    set "NDK=C:\Users\aleks\OneDrive\Desktop\android-ndk-r26b\toolchains\llvm\prebuilt\windows-x86_64"
)
set "CLANG=%NDK%\bin\clang++.exe"
set "SYSROOT=%NDK%\sysroot"
set "FLAGS=--target=x86_64-linux-android30 --sysroot=%SYSROOT% -static -O2 -std=c++17 -fno-exceptions -fno-rtti"

if not exist "%CLANG%" (
    echo [-] clang++ not found: "%CLANG%"
    exit /b 1
)
if not exist "%ROOT%game_offsets.h" (
    echo [-] game_offsets.h not found: "%ROOT%game_offsets.h"
    exit /b 1
)

echo [*] Building daemon...
"%CLANG%" %FLAGS% -o "%ROOT%daemon" "%ROOT%daemon.cpp"
if errorlevel 1 (
    echo [-] daemon build failed
    exit /b 1
)
echo [+] daemon OK

echo [+] Build complete
