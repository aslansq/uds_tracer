@echo off
setlocal enabledelayedexpansion

set THIS_DIR=%~dp0
cd !THIS_DIR!

if exist build (
	rmdir /s /q build
)

mkdir build
cd build
qmake ../*.pro
if !errorlevel! equ 0 (
    echo Success qmake
) else (
    echo Failure qmake
    goto ungracefulExit
)

mingw32-make
if !errorlevel! equ 0 (
    echo Success mingw32-make
) else (
    echo Failure mingw32-make
    goto ungracefulExit
)

cd !THIS_DIR!
echo Build completed successfully.
exit /b 0


:ungracefulExit
set /P ans="Press enter key to continue."
exit /b 1