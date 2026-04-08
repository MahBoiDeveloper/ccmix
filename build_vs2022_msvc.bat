@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "BUILD_DIR=%SCRIPT_DIR%\build-vs2022"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"

set "PLATFORM=%~2"
if not defined PLATFORM set "PLATFORM=x64"

set "CRYPTOPP_ROOT_DIR=%~3"
if not defined CRYPTOPP_ROOT_DIR if defined CRYPTOPPROOT set "CRYPTOPP_ROOT_DIR=%CRYPTOPPROOT%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Could not find vswhere.exe.
    echo [ERROR] Expected path: "%VSWHERE%"
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VSINSTALLDIR=%%I"
)

if not defined VSINSTALLDIR (
    echo [ERROR] Visual Studio 2022 with MSVC build tools was not found.
    exit /b 1
)

call "%VSINSTALLDIR%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo [ERROR] Failed to initialize the Visual Studio 2022 MSVC environment.
    exit /b 1
)

set "CMAKE_EXE=cmake"
where /q "%CMAKE_EXE%"
if errorlevel 1 (
    set "CMAKE_EXE=%VSINSTALLDIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

if not exist "%CMAKE_EXE%" if /i not "%CMAKE_EXE%"=="cmake" (
    echo [ERROR] CMake was not found.
    exit /b 1
)

if defined CRYPTOPP_ROOT_DIR (
    set "CRYPTOPP_ARG=-DCRYPTOPP_ROOT_DIR=%CRYPTOPP_ROOT_DIR%"
) else (
    set "CRYPTOPP_ARG="
)

echo [INFO] Source: "%SCRIPT_DIR%"
echo [INFO] Build directory: "%BUILD_DIR%"
echo [INFO] Configuration: "%CONFIG%"
echo [INFO] Platform: "%PLATFORM%"
if defined CRYPTOPP_ROOT_DIR (
    echo [INFO] Crypto++ root: "%CRYPTOPP_ROOT_DIR%"
) else (
    echo [INFO] Crypto++ root: not set, relying on CRYPTOPPROOT or CMake discovery
)

if defined CRYPTOPP_ARG (
    "%CMAKE_EXE%" -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A "%PLATFORM%" "%CRYPTOPP_ARG%"
) else (
    "%CMAKE_EXE%" -S "%SCRIPT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A "%PLATFORM%"
)
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    exit /b 1
)

"%CMAKE_EXE%" --build "%BUILD_DIR%" --config "%CONFIG%"
if errorlevel 1 (
    echo [ERROR] CMake build failed.
    exit /b 1
)

echo [INFO] Build completed successfully.
exit /b 0
