@echo off
echo ========================================
echo   XSHM All Stress Tests Comparison
echo ========================================
echo.

echo This will run 3 different stress tests:
echo 1. Comprehensive Test (with console output)
echo 2. Background Test (no console output)  
echo 3. Silent Test (absolutely minimal)
echo.

pause

echo.
echo ========================================
echo TEST 1: Comprehensive Test
echo ========================================
echo.
echo Building comprehensive test...
bcc64x comprehensive_analysis.cpp ..\xshm.obj -o comprehensive_analysis.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build comprehensive test
    pause
    exit /b 1
)

echo Running comprehensive test (75 seconds)...
comprehensive_analysis.exe

echo.
echo ========================================
echo TEST 2: Background Stress Test
echo ========================================
echo.
echo Building background stress test...
bcc64x background_stress_test.cpp ..\xshm.obj -o background_stress_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build background stress test
    pause
    exit /b 1
)

echo Running background stress test (30 seconds)...
background_stress_test.exe

echo.
echo ========================================
echo TEST 3: Silent Stress Test
echo ========================================
echo.
echo Building silent stress test...
bcc64x silent_stress_test.cpp ..\xshm.obj -o silent_stress_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build silent stress test
    pause
    exit /b 1
)

echo Running silent stress test (30 seconds)...
silent_stress_test.exe

echo.
echo ========================================
echo ALL TESTS COMPLETED!
echo ========================================
echo.
echo Results files:
echo - detailed_analysis_report.txt (comprehensive)
echo - background_stress_results.txt (background)
echo - silent_stress_results.txt (silent)
echo.
echo Compare the performance results!
echo.
pause
