@echo off
setlocal
set "ROOT=%~dp0"
cd /d "%ROOT%"

echo ===============================================
echo [1/4] Building Android daemon (NDK)...
echo ===============================================
call "%ROOT%daemon_src\build.bat"
if errorlevel 1 (
    echo [-] Daemon build failed. Aborting.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo [2/4] Embedding daemon into daemon_raw.h ...
echo ===============================================
python "%ROOT%bin2h.py"
if errorlevel 1 (
    echo [-] Daemon embed failed. Aborting.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo [3/4] Embedding ADB tools into adb_raw.h ...
echo ===============================================
powershell -NoProfile -ExecutionPolicy Bypass -File "%ROOT%make_adb_raw.ps1"
if errorlevel 1 (
    echo [-] ADB embed failed. Aborting.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo [4/4] Building Phantom.exe (MSVC) ...
echo ===============================================
call "%ROOT%build.bat"
if errorlevel 1 (
    echo [-] Phantom build failed.
    pause
    exit /b 1
)

echo.
echo ===============================================
echo [+] Full build complete: Phantom.exe
echo ===============================================
pause
