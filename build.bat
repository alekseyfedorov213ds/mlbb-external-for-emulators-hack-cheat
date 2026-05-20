@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"

REM === Regenerate icon every build (silent unless it fails) ===
powershell -NoProfile -ExecutionPolicy Bypass -File make_icon.ps1 >nul
if %ERRORLEVEL% NEQ 0 (
    echo Icon generation failed!
    pause
    exit /b 1
)

rc /nologo /fo phantom.res phantom.rc
if %ERRORLEVEL% NEQ 0 (
    echo Resource compilation failed!
    pause
    exit /b 1
)
cl /std:c++17 /MT /EHsc /O2 /Oi /Os /GS /GL /Gw /Gy /guard:cf /W3 /wd4312 /D_WINSOCKAPI_ /DIMGUI_DEFINE_MATH_OPERATORS /Fe:Phantom.exe main.cpp src/core/globals.cpp src/core/config.cpp src/game/mlbb.cpp src/overlay/widgets.cpp src/overlay/esp.cpp src/overlay/menu.cpp src/overlay/chat_ui.cpp src/overlay/renderer.cpp src/loader/auth.cpp src/loader/ui_login.cpp src/daemon/tcp_client.cpp src/daemon/adb_runner.cpp src/app.cpp imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_dx11.cpp imgui/backends/imgui_impl_win32.cpp imgui/stb_image.cpp /I. /Isrc /Iimgui /Iimgui/backends /Iicons_data /DUNICODE /D_UNICODE /link /LTCG /OPT:REF /OPT:ICF /GUARD:CF /DEBUG:NONE /PDBALTPATH:none phantom.res d3d11.lib dxgi.lib dwmapi.lib ws2_32.lib wininet.lib winhttp.lib shell32.lib advapi32.lib
if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)
echo Build success: Phantom.exe
pause
