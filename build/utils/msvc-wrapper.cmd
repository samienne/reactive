@echo off
setlocal EnableDelayedExpansion

:: === Parse arguments ===
set "MSVC_VERSION=%1"
set "ARCH=%2"
set "TOOL=%3"
shift
shift
shift

set RESTVAR=
:loop1
if "%1"=="" goto after_loop
set "RESTVAR=%RESTVAR% %1"
shift
goto loop1

:after_loop

if "%MSVC_VERSION%"=="" (
    echo Usage: msvc-wrapper.cmd [msvc_version] [arch] [tool.exe] [args...]
    echo.
    echo Examples:
    echo    msvc-wrapper.cmd 2022 x64 cl.exe /nologo /?
    echo.
    echo Supported MSVC versions: 2022, 2019, 2017
    echo Supported architectures: x64, x86, arm64
    exit /b 1
)

if "%ARCH%"=="" (
    echo Error: Architecture must be specified: x64, x86, arm64
    exit /b
)

if "%TOOL%"=="" (
    echo Error: No tool specified.
    exit /b 1
)

set "MSVC_VERSION_NUMBER="
if "%MSVC_VERSION%"=="2019" (
    set "MSVC_VERSION_NUMBER=16"
) else (
    echo error: Unknown msvc version"
    exit /b 1
)

set "VSWHERE="
for %%x in ("%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe") do (
    if exist "%%~fX" (
        set "VSWHERE=%%~fX"
    )
)

if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
)

@rem echo VSWHERE = %VSWHERE%

:: === Detect vswhere in PATH ===
set "VS_INSTALL_PATH="
if defined VSWHERE (
    for /f "usebackq tokens=*" %%I in (`"%VSWHERE%" -products * -version %MSVC_VERSION_NUMBER% -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest`) do (
        set "VS_INSTALL_PATH=%%I"
    )
)

@rem where vswhere > nul 2>&1
@rem if not errorlevel 1 (
    @rem for /f "usebackq tokens=*" %%I in ('c:^\Program Files (x86)^\Microsoft Visual Studio^\Installer^\vswhere.exe -products * -version [%MSVC_VERSION%].0 -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath -latest') do (
        @rem set "VS_INSTALL_PATH=%%I"
    @rem )
@rem )

:: === Fallbacks if vswhere didn't work ===
if not defined VS_INSTALL_PATH (
    if "%MSVC_VERSION%"=="2022" (
        set "VS_INSTALL_PATH=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
    ) else if "%MSVC_VERSION%"=="2019" (
        set "VS_INSTALL_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools"
    ) else if "%MSVC_VERSION%"=="2017" (
        set "VS_INSTALL_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools"
    ) else (
        echo [msvc-wrapper] Could not find Visual Studio for version %MSVC_VERSION%
        exit /b 1
    )
)

:: === Build paths ===
set "VCVARS_PATH=%VS_INSTALL_PATH%\VC\Auxiliary\Build\vcvarsall.bat"
set "BUILD_DIR=%CD%"
set "ENV_FILE=%BUILD_DIR%\env-%MSVC_VERSION%-%ARCH%.txt"

@rem echo path "%VS_INSTALL_PATH%"
@rem echo builddir "%BUILD_DIR%"

:: === Load or create environment ===
if not defined MSVC_ENV_LOADED (
    if exist "%ENV_FILE%" (
        for /f "tokens=1,* delims==" %%A in (%ENV_FILE%) do (
            set "%%A=%%B"
        )
        set "TOOL=%TOOL%"
        set "RESTVAR=%RESTVAR%"
    ) else (
        @rem echo Generating MSVC %MSVC_VERSION% environment for %ARCH% in %ENV_FILE% ...
        @rem echo call "%VCVARS_PATH%" %ARCH%
        call "%VCVARS_PATH%" %ARCH% > nul
        set "TOOL="
        set "RESTVAR="
        set > "%ENV_FILE%"
        set "TOOL=%TOOL%"
        set "RESTVAR=%RESTVAR%"
    )
    set "MSVC_ENV_LOADED=1"
)

:: === Locate the tool ===
where "%TOOL%" > nul 2>&1
if errorlevel 1 (
    echo Error: Tool not found in environment: "%TOOL%"
    exit /b 1
)

:: === Forward to the tool ===
"%TOOL%" %RESTVAR%

endlocal

exit
