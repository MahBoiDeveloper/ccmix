@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "BUILD_DIR=%SCRIPT_DIR%\build"
set "CONFIG=%~1"
if not defined CONFIG set "CONFIG=Release"

set "PLATFORM=%~2"
if not defined PLATFORM set "PLATFORM=x64"

set "CRYPTOPP_ROOT_DIR=%~3"
if not defined CRYPTOPP_ROOT_DIR if defined CRYPTOPPROOT set "CRYPTOPP_ROOT_DIR=%CRYPTOPPROOT%"
set "VENDORED_CRYPTOPP=%SCRIPT_DIR%\libs\cryptopp\cryptlib.vcxproj"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo [ERROR] Could not find vswhere.exe.
    echo [ERROR] Expected path: "%VSWHERE%"
    exit /b 1
)

for /f "usebackq delims=" %%I in (`"%VSWHERE%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS2022_INSTALL_DIR=%%I"
)

if not defined VS2022_INSTALL_DIR (
    echo [ERROR] Visual Studio 2022 with MSVC build tools was not found.
    exit /b 1
)

set "VSDEVCMD=%VS2022_INSTALL_DIR%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
    echo [ERROR] Could not find VsDevCmd.bat.
    echo [ERROR] Expected path: "%VSDEVCMD%"
    exit /b 1
)

set "VSCMD_HOST_ARCH=x64"
set "VSCMD_TARGET_ARCH=%PLATFORM%"
if /i "%PLATFORM%"=="Win32" set "VSCMD_TARGET_ARCH=x86"

call "%VSDEVCMD%" -no_logo -host_arch=%VSCMD_HOST_ARCH% -arch=%VSCMD_TARGET_ARCH%
if errorlevel 1 (
    echo [ERROR] Failed to initialize the Visual Studio 2022 MSVC environment via VsDevCmd.bat.
    exit /b 1
)

set "CMAKE_EXE=cmake"
where /q "%CMAKE_EXE%"
if errorlevel 1 (
    set "CMAKE_EXE=%VS2022_INSTALL_DIR%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
)

if not exist "%CMAKE_EXE%" if /i not "%CMAKE_EXE%"=="cmake" (
    echo [ERROR] CMake was not found.
    exit /b 1
)

if exist "%VENDORED_CRYPTOPP%" (
    set "CRYPTOPP_ARG="
    set "CRYPTOPP_MESSAGE=[INFO] Crypto++ source: vendored submodule at %SCRIPT_DIR%\libs\cryptopp"
) else (
    if defined CRYPTOPP_ROOT_DIR (
        set "CRYPTOPP_ARG=-DCRYPTOPP_ROOT_DIR=%CRYPTOPP_ROOT_DIR%"
        set "CRYPTOPP_MESSAGE=[INFO] Crypto++ source: external root at %CRYPTOPP_ROOT_DIR%"
    ) else (
        set "CRYPTOPP_ARG="
        set "CRYPTOPP_MESSAGE=[INFO] Crypto++ source: not set, relying on CRYPTOPPROOT or CMake discovery"
    )
)

echo [INFO] Source: "%SCRIPT_DIR%"
echo [INFO] Build directory: "%BUILD_DIR%"
echo [INFO] Configuration: "%CONFIG%"
echo [INFO] Platform: "%PLATFORM%"
echo %CRYPTOPP_MESSAGE%

if exist "%BUILD_DIR%\CMakeCache.txt" (
    findstr /b /c:"CMAKE_GENERATOR:INTERNAL=Visual Studio 17 2022" "%BUILD_DIR%\CMakeCache.txt" >nul
    if errorlevel 1 (
        echo [INFO] Resetting "%BUILD_DIR%" because it was configured with a different generator.
        rmdir /s /q "%BUILD_DIR%"
    ) else (
        findstr /b /c:"CMAKE_GENERATOR_PLATFORM:INTERNAL=%PLATFORM%" "%BUILD_DIR%\CMakeCache.txt" >nul
        if errorlevel 1 (
            echo [INFO] Resetting "%BUILD_DIR%" because it was configured for a different platform.
            rmdir /s /q "%BUILD_DIR%"
        )
    )
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
