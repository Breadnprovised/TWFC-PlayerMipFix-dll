@echo off
setlocal
cd /d "%~dp0"
if not exist build\patch_tests.exe (
  echo Run build.cmd first.
  exit /b 1
)
build\patch_tests.exe
exit /b %errorlevel%
