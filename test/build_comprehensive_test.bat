@echo off
echo ========================================
echo    XSHM Comprehensive Test Builder
echo ========================================
echo.

echo Building XSHM library...
cd ..
bcc64x -c xshm.cpp -o xshm.obj
if %errorlevel% neq 0 (
    echo ERROR: Failed to build XSHM library
    pause
    exit /b 1
)
echo ✅ XSHM library built successfully
echo.

cd test

echo Building comprehensive analysis test...
bcc64x comprehensive_analysis.cpp ..\xshm.obj -o comprehensive_analysis.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build comprehensive analysis test
    pause
    exit /b 1
)
echo ✅ Comprehensive analysis test built successfully
echo.

echo ========================================
echo All tests built successfully!
echo ========================================
echo.
echo Available test:
echo   - comprehensive_analysis.exe (comprehensive test with stress test)
echo.
echo To run test:
echo   comprehensive_analysis.exe
echo.
echo The test will run for 60 seconds and generate:
echo   - detailed_analysis_report.txt
echo   - summary_analysis_report.txt
echo.
pause
