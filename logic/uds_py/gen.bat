:: filepath: c:\Users\mehmet.aslan\Desktop\uds_tracer\logic\uds_py\gen.bat
@echo off

:: Remove and recreate the build directory
if exist build (
    rmdir /s /q build
)
mkdir build

:: Remove and recreate the output directory
if exist output (
    rmdir /s /q output
)
mkdir output

:: Run the Python script
C:\Users\mehmet.aslan\AppData\Local\Programs\Python\Python311\python uds.py
