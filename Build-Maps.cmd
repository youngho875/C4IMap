@echo off
setlocal
set "MAP_MODE=%~1"
if "%MAP_MODE%"=="" set "MAP_MODE=All"
set "MAP_CONFIG=%~2"
if "%MAP_CONFIG%"=="" set "MAP_CONFIG=Release"
if /I not "%MAP_CONFIG%"=="Release" if /I not "%MAP_CONFIG%"=="Debug" exit /b 2
if /I not "%MAP_MODE%"=="Local" if /I not "%MAP_MODE%"=="Tiles" if /I not "%MAP_MODE%"=="Network" if /I not "%MAP_MODE%"=="All" exit /b 2
for /f "usebackq delims=" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MAP_MSBUILD=%%I"
if not defined MAP_MSBUILD exit /b 3
if /I "%MAP_MODE%"=="All" (
    for %%M in (Local Tiles Network) do (
        call :BuildMode %%M
        if errorlevel 1 exit /b 1
    )
    exit /b 0
)
call :BuildMode %MAP_MODE%
exit /b %errorlevel%
:BuildMode
"%MAP_MSBUILD%" "%~dp0C4IMap.%1.vcxproj" /t:Build /p:Configuration=%MAP_CONFIG% /p:Platform=x64 /m /v:minimal /nologo
exit /b %errorlevel%
