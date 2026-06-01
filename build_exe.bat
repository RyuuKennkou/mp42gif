@echo off
echo ============================================
echo  MP4 to GIF - Build EXE
echo ============================================
echo.

python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Python not found
    pause & exit /b 1
)

echo [1/4] Installing dependencies...
pip install -r requirements.txt pyinstaller
if %errorlevel% neq 0 ( pause & exit /b 1 )

echo.
echo [2/4] Generating icon...
if not exist icon.ico python make_icon.py

echo.
echo [3/4] Building EXE...
pyinstaller --onefile --windowed --icon=icon.ico --name mp4_to_gif mp4_to_gif.py
if %errorlevel% neq 0 ( pause & exit /b 1 )

echo.
echo [4/4] Done! EXE: dist\mp4_to_gif.exe
echo ============================================
pause
