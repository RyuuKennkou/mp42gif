@echo off
echo ============================================
echo  MP4 to GIF v3 - C++ Build
echo ============================================
echo.

:: 查找 VS 环境 (优先用 vswhere, 回退到手动路径)
where cl.exe >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] MSVC compiler found
) else (
    echo.
    echo [ERROR] 未找到 Visual Studio C++ 编译器。
    echo.
    echo 请先安装 Visual Studio Build Tools (免费):
    echo   https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022
    echo.
    echo 安装时勾选 "使用C++的桌面开发" 工作负荷。
    echo 安装完成后，从开始菜单打开 "Developer Command Prompt for VS"，
    echo cd 到本目录，然后运行: build_v3.bat
    echo.
    pause
    exit /b 1
)

echo.
echo Compiling mp4_to_gif_v3.exe ...
echo.

cl /MT /O2 /EHsc /Fe:mp4_to_gif_v3.exe mp4_to_gif_v3.cpp ^
    mfplat.lib mfreadwrite.lib mfuuid.lib gdiplus.lib ^
    comctl32.lib comdlg32.lib ole32.lib

if %errorlevel% equ 0 (
    echo.
    echo ============================================
    echo  Build OK: mp4_to_gif_v3.exe (~200 KB)
    echo ============================================
    dir mp4_to_gif_v3.exe
) else (
    echo [BUILD FAILED]
)
pause
