@echo off
setlocal

chcp 65001 > nul

set "ROOT_DIR=%~dp0"
set "BUILD_DIR=%ROOT_DIR%build"
set "WWMIX_EXE=%BUILD_DIR%\bin\Release\wwmix.exe"
set "TEST_SCRIPT=%ROOT_DIR%tests\Run-WwMixTests.ps1"

if not exist "%TEST_SCRIPT%" (
    echo Test runner was not found: "%TEST_SCRIPT%"
    exit /b 1
)

if not exist "%WWMIX_EXE%" (
    echo wwmix.exe was not found. Building the project...

    if not exist "%BUILD_DIR%\CMakeCache.txt" (
        cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
        if errorlevel 1 (
            echo CMake configuration failed.
            exit /b 1
        )
    )

    cmake --build "%BUILD_DIR%" --config Release --target wwmix
    if errorlevel 1 (
        echo Project build failed.
        exit /b 1
    )
) else (
    echo Using existing build: "%WWMIX_EXE%"
)

powershell -ExecutionPolicy Bypass -File "%TEST_SCRIPT%" -WwMixPath "%WWMIX_EXE%" %*
set "TEST_EXIT_CODE=%ERRORLEVEL%"

exit /b %TEST_EXIT_CODE%
