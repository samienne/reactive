@echo off
setlocal EnableExtensions EnableDelayedExpansion
rem loomworks repo-local launcher (Windows). Committed alongside lw.pin. Fetches
rem the pinned, verified lw host binary into .nvim\cache\ and runs it; the host
rem provisions the pinned bundle itself. Regenerate with `lw update`.
rem Windows system tools (find, findstr, certutil, curl, where) are called by
rem their absolute %SystemRoot%\System32 path: a bare name can resolve to a
rem same-named tool earlier on PATH (Git's usr/bin/find under Git Bash / CI).

if not "%LOOMWORKS_LW%"=="" (
  "%LOOMWORKS_LW%" %*
  exit /b !ERRORLEVEL!
)

set "here=%~dp0"
set "pin=%here%lw.pin"
if not exist "%pin%" ( echo lw: no lw.pin next to this launcher 1>&2 & exit /b 1 )

set "asset=lw-windows-x86_64.exe"
if /I "%PROCESSOR_ARCHITECTURE%"=="ARM64" (
  echo lw: no pinned lw binary for windows/arm64 1>&2 & exit /b 1
)

set "version="
set "want="
for /f "usebackq tokens=1,* delims== " %%A in ("%pin%") do (
  set "k=%%A"
  set "v=%%B"
  if /I "!k!"=="version" set "version=!v!"
  if /I "!k!"=="sha256_%asset%" set "want=!v!"
)
if "!version!"=="" ( echo lw: lw.pin has no version 1>&2 & exit /b 1 )
if "!want!"=="" ( echo lw: lw.pin has no sha256 for %asset% 1>&2 & exit /b 1 )
rem reject a malicious pinned version before it reaches a URL (a repo must not
rem be able to redirect the fetch): forbid anything outside [-0-9A-Za-z._+] or `..`
echo(!version!| "%SystemRoot%\System32\findstr.exe" /r /c:"[^-0-9A-Za-z._+]" >nul && ( echo lw: invalid pinned version !version! 1>&2 & exit /b 1 )
echo(!version!| "%SystemRoot%\System32\findstr.exe" /c:".." >nul && ( echo lw: invalid pinned version !version! 1>&2 & exit /b 1 )

set "cache=%here%.nvim\cache"
set "bin=%cache%\lw-!version!-%asset%"

rem detect launcher-only flags without shifting (so phase 2 still sees all args)
set "insecure=0"
if "%LOOMWORKS_INSECURE%"=="1" set "insecure=1"
set "do_verify=0"
for %%A in (%*) do (
  if /I "%%~A"=="--insecure" set "insecure=1"
  if /I "%%~A"=="--verify" set "do_verify=1"
)

set "ok=0"
if exist "%bin%" ( call :sha "%bin%" & if /I "!got!"=="!want!" set "ok=1" )
if "!ok!"=="1" goto forward

del /f /q "%bin%" 2>nul
if not exist "%cache%" mkdir "%cache%"
if defined LOOMWORKS_RELEASE_URL (
  set "url=%LOOMWORKS_RELEASE_URL%/%asset%"
) else (
  set "url=https://github.com/samienne/loomworks.nvim/releases/download/v!version!/%asset%"
)
echo lw: fetching pinned lw !version! ^(%asset%^)... 1>&2
set "tmp=%bin%.dl"
echo(!url!| "%SystemRoot%\System32\find.exe" "://" >nul
if errorlevel 1 (
  rem bare path / offline mirror: copy instead of curl (matches the host)
  set "src=!url:/=\!"
  copy /y "!src!" "%tmp%" >nul
) else (
  set "kflag="
  if "!insecure!"=="1" set "kflag=-k"
  "%SystemRoot%\System32\curl.exe" -fL !kflag! -o "%tmp%" "!url!"
)
if errorlevel 1 ( echo lw: download failed: !url! 1>&2 & del /f /q "%tmp%" 2>nul & exit /b 1 )
call :sha "%tmp%"
if /I not "!got!"=="!want!" (
  echo lw: sha256 mismatch for %asset% ^(pin !want!, got !got!^) -- aborting 1>&2
  del /f /q "%tmp%" 2>nul & exit /b 1
)
move /y "%tmp%" "%bin%" >nul
> "%cache%\lw.marker" (
  echo version=!version!
  echo asset=%asset%
  echo sha256=!want!
)

:forward
if "!do_verify!"=="1" (
  "%SystemRoot%\System32\where.exe" gh >nul 2>nul
  if errorlevel 1 (
    echo lw: --verify: gh not found; skipping attestation ^(sha256 already verified^) 1>&2
  ) else (
    gh attestation verify "%bin%" --repo samienne/loomworks.nvim || exit /b 1
  )
)
rem Forward args with delayed expansion OFF so a forwarded arg containing `!`
rem survives; re-peel the launcher-only flags from the untouched arg list.
set "PINVER=!version!"
set "PINBIN=!bin!"
setlocal DisableDelayedExpansion
set "LOOMWORKS_PINNED=%PINVER%"
set "LW_ROOT=%CD%"
set "fwd="
:peel
if "%~1"=="" goto peeled
if /I "%~1"=="--insecure" ( shift & goto peel )
if /I "%~1"=="--verify" ( shift & goto peel )
set "fwd=%fwd% %1"
shift
goto peel
:peeled
"%PINBIN%"%fwd%
exit /b %ERRORLEVEL%

:sha
set "got="
rem the outer quote pair keeps cmd /c from stripping the program path's quotes
for /f "skip=1 delims=" %%H in ('""%SystemRoot%\System32\certutil.exe" -hashfile "%~1" SHA256"') do if not defined got set "got=%%H"
set "got=!got: =!"
goto :eof
