@REM Copyright (c) Microsoft. All rights reserved.
@REM Licensed under the MIT license. See LICENSE file in the project root for full license information.

@setlocal EnableExtensions EnableDelayedExpansion
@echo off

rem -----------------------------------------------------------------------------
rem -- setup path information
rem -----------------------------------------------------------------------------

set current-path=%~dp0
rem // remove trailing slash
set current-path=%current-path:~0,-1%

echo Current Path: %current-path%

set build-root=%current-path%\..\..\..
rem // resolve to fully qualified path
for %%i in ("%build-root%") do set build-root=%%~fi

set client-root=%current-path%\..\..\..
for %%i in ("%client-root%") do set client-root=%%~fi

rem -----------------------------------------------------------------------------
rem -- check prerequisites and clean directories
rem -----------------------------------------------------------------------------

where /q nuget.exe
if not !errorlevel! == 0 (
@Echo Azure Amqp needs to download nuget.exe from https://www.nuget.org/nuget.exe 
@Echo https://www.nuget.org 
choice /C yn /M "Do you want to download and run nuget.exe?" 
if not !errorlevel!==1 goto :eof
rem if nuget.exe is not found, then ask user
Powershell.exe wget -outf nuget.exe https://nuget.org/nuget.exe
	if not exist .\nuget.exe (
		echo nuget does not exist
		exit /b 1
	)
)

if exist %USERPROFILE%\azure_amqp_nuget (
	rmdir /s/q %USERPROFILE%\azure_amqp_nuget
	rem no error checking
)

if exist %client-root%\azure_amqp_output (
	rmdir /s/q %client-root%\azure_amqp_output
	rem no error checking
)

rem -----------------------------------------------------------------------------
rem -- build with CMAKE
rem -----------------------------------------------------------------------------

echo Build root is %build-root%
echo Client root is %client-root%

mkdir %USERPROFILE%\azure_amqp_nuget
rem no error checking

pushd %USERPROFILE%\azure_amqp_nuget

rem Build Win32
cmake %build-root%
if not %errorlevel%==0 exit /b %errorlevel%

call :_run-msbuild "Build" uamqp.sln Debug Win32
if not %errorlevel%==0 exit /b %errorlevel%

rem -- Copy all Win32 files from cmake build directory to the repo directory
xcopy /q /y /R %USERPROFILE%\azure_amqp_nuget\Debug\*.* %client-root%\azure_amqp_output\win32\debug\*.*
if %errorlevel% neq 0 exit /b %errorlevel%

call :_run-msbuild "Build" uamqp.sln Release Win32
if not %errorlevel%==0 exit /b %errorlevel%

rem -- Copy all Win32 Release files from cmake build directory to the repo directory
xcopy /q /y /R %USERPROFILE%\azure_amqp_nuget\Release\*.* %client-root%\azure_amqp_output\win32\Release\*.*
if %errorlevel% neq 0 exit /b %errorlevel%

rem -- Remove the x86 cmake files
rmdir /s/q %USERPROFILE%\azure_amqp_nuget
rem no error checking

rem -----------------------------------------------------------------------------
rem -- build with CMAKE x64
rem -----------------------------------------------------------------------------

cmake %build-root% -G "Visual Studio 14 Win64"
if not %errorlevel%==0 exit /b %errorlevel%

call :_run-msbuild "Build" uamqp.sln Debug x64
if not %errorlevel%==0 exit /b %errorlevel%

rem -- Copy all x64 files from cmake build directory to the repo directory
xcopy /q /y /R %USERPROFILE%\azure_amqp_nuget\Debug\*.* %client-root%\azure_amqp_output\x64\debug\*.*
if %errorlevel% neq 0 exit /b %errorlevel%

call :_run-msbuild "Build" uamqp.sln Release x64
if not %errorlevel%==0 exit /b %errorlevel%

rem -- Copy all x64 Release files from cmake build directory to the repo directory
xcopy /q /y /R %USERPROFILE%\azure_amqp_nuget\Release\*.* %client-root%\azure_amqp_output\x64\Release\*.*
if %errorlevel% neq 0 exit /b %errorlevel%

if exist *.nupkg (
	del *.nupkg
)

popd

rem -- Package Nuget
nuget pack %build-root%\build_all\packaging\windows\Microsoft.Azure.uamqp.nuspec -OutputDirectory %build-root%\build_all\packaging\windows

rmdir /s/q %client-root%\azure_amqp_output
rmdir /s/q %USERPROFILE%\azure_amqp_nuget

popd
goto :eof

rem -----------------------------------------------------------------------------
rem -- helper subroutines
rem -----------------------------------------------------------------------------

:_run-msbuild
rem // optionally override configuration|platform
setlocal EnableExtensions
set build-target=
if "%~1" neq "Build" set "build-target=/t:%~1"
if "%~3" neq "" set build-config=%~3
if "%~4" neq "" set build-platform=%~4

msbuild /m %build-target% "/p:Configuration=%build-config%;Platform=%build-platform%" %2
if not %errorlevel%==0 exit /b %errorlevel%
goto :eof

echo done