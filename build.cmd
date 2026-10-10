@echo off
setlocal
cd /d "%~dp0"
if defined VSCMD_ARG_TGT_ARCH goto compile
set "MIPFIX_VS="
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "MIPFIX_VS=%%i"
if not defined MIPFIX_VS (
  echo Install Visual Studio C++ Desktop Development with x86 tools and Windows SDK.
  exit /b 1
)
call "%MIPFIX_VS%\VC\Auxiliary\Build\vcvarsall.bat" x86
if errorlevel 1 exit /b 1
:compile
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" (
  echo Use an x86 developer prompt, not x64.
  exit /b 1
)
if not exist build mkdir build
cl /nologo /std:c++17 /W4 /WX /O2 /MT /EHsc /LD src\dll.cpp src\patch.cpp /Fo:build\ /link /MACHINE:X86 /DEF:src\exports.def /OUT:TWFC_PlayerMipFix.dll /IMPLIB:build\TWFC_PlayerMipFix.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /W4 /WX /O2 /MT /EHsc tests\patch_tests.cpp src\patch.cpp /Fo:build\ /Fe:build\patch_tests.exe /link /MACHINE:X86
if errorlevel 1 exit /b 1
echo Built TWFC_PlayerMipFix.dll and build\patch_tests.exe.
echo Run test.cmd to execute the patch validation tests.
exit /b 0
