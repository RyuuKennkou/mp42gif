@echo off
echo ============================================
echo  MP4 to GIF v2 - Build
echo ============================================
echo.

set CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe
if not exist "%CSC%" (
    echo [ERROR] csc.exe not found at %CSC%
    pause
    exit /b 1
)

echo Compiling mp4_to_gif_v2.exe ...
"%CSC%" /target:winexe /out:mp4_to_gif_v2.exe mp4_to_gif_v2.cs

if %errorlevel% equ 0 (
    echo.
    echo ============================================
    echo  Build OK: mp4_to_gif_v2.exe
    echo ============================================
) else (
    echo [BUILD FAILED]
)
pause
