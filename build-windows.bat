@echo off
setlocal
cd /d "%~dp0"
where py >nul 2>nul
if errorlevel 1 (
  echo Build requires 64-bit Python 3.12 from python.org. End users do not need Python.
  pause
  exit /b 1
)
py -3.12 -m venv .venv-build
if errorlevel 1 goto :failed
.venv-build\Scripts\python.exe -m pip install -e . pyinstaller==6.16.0
if errorlevel 1 goto :failed
.venv-build\Scripts\python.exe -c "import struct; assert struct.calcsize('P') == 8, '64-bit Python required'"
if errorlevel 1 goto :failed
set QT_QPA_PLATFORM=offscreen
.venv-build\Scripts\python.exe -m unittest discover -s tests -v
if errorlevel 1 goto :failed
set QT_QPA_PLATFORM=
.venv-build\Scripts\python.exe -m PyInstaller --noconfirm --clean --windowed --onedir --name LanLink launch.py
if errorlevel 1 goto :failed
powershell -NoProfile -File packaging\finish-portable.ps1
if errorlevel 1 goto :failed
echo Portable ZIP built in dist. Review packaging\WINDOWS.md before distribution.
pause
exit /b 0
:failed
echo BUILD FAILED. No validated release produced.
pause
exit /b 1
