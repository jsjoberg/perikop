@echo off
cmake -S . -B build/cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b %errorlevel%
cmake --build build/cmake --parallel
