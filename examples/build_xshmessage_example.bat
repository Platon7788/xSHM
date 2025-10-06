@echo off
echo Building XSHMessage Example...

REM Compile the example
cl /EHsc /std:c++17 /I.. xshmessage_example.cpp ..\xshm.cpp /Fe:xshmessage_example.exe

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ✅ XSHMessage example compiled successfully!
    echo.
    echo To run the example:
    echo   xshmessage_example.exe
    echo.
) else (
    echo.
    echo ❌ Failed to compile XSHMessage example
    echo.
)

pause
