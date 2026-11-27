@echo off
rem Compile la bibliotheque native (Windows, Visual Studio Build Tools x64).
rem Sortie : build\clock3ds_textures.dll (a copier a cote du .nrm dans le dossier mods).
rem "build.bat test" compile aussi build\clock3ds_textures_test.exe (extraction des pieces en .rgba).
setlocal
set "HERE=%~dp0"
if not exist "%HERE%..\build" mkdir "%HERE%..\build"
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`call "%VSWHERE%" -latest -products * -property installationPath`) do set "VS=%%i"
if not defined VS (
    echo Visual Studio Build Tools introuvable.
    exit /b 1
)
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
set "FLAGS=/nologo /O2 /EHsc /std:c++17 /utf-8 /W3 /MT /D_CRT_SECURE_NO_WARNINGS"
cl %FLAGS% /LD "%HERE%clock3ds_textures.cpp" /Fe"%HERE%..\build\clock3ds_textures.dll" /Fo"%HERE%..\build\\" /link /NOIMPLIB /NOEXP || exit /b 1
if "%1"=="test" (
    cl %FLAGS% /DCLOCK3DS_TEST "%HERE%clock3ds_textures.cpp" /Fe"%HERE%..\build\clock3ds_textures_test.exe" /Fo"%HERE%..\build\\" || exit /b 1
)
echo OK
