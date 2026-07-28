@echo off
setlocal

if exist build (
    rmdir /s /q build
)

cmake -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :error

cmake --build build --config Release --parallel 4
if errorlevel 1 goto :error

echo.
echo Build succeeded.
goto :eof

:error
echo.
echo Build failed.
exit /b 1
