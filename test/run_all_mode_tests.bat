@echo off
echo ========================================
echo XSHM Comprehensive Mode Testing Suite
echo ========================================
echo.

echo Building all test executables...
echo.

REM Build background stress test (async mode)
echo [1/4] Building background stress test (async mode)...
bcc64x background_stress_test.cpp ..\xshm.obj -o background_stress_async.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build background stress test (async)
    pause
    exit /b 1
)

REM Build background stress test (sync mode) - we'll use the same exe with --sync flag
echo [2/4] Background stress test supports both modes via command line
echo.

REM Build comprehensive mode test
echo [3/4] Building comprehensive mode test...
bcc64x comprehensive_mode_test.cpp ..\xshm.obj -o comprehensive_mode_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build comprehensive mode test
    pause
    exit /b 1
)

REM Build silent stress test
echo [4/4] Building silent stress test...
bcc64x silent_stress_test.cpp ..\xshm.obj -o silent_stress_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build silent stress test
    pause
    exit /b 1
)

echo.
echo ========================================
echo All executables built successfully!
echo ========================================
echo.

echo Available tests:
echo 1. background_stress_async.exe          - ASYNC mode (no confirmation)
echo 2. background_stress_async.exe --sync   - SYNC mode (with confirmation)
echo 3. comprehensive_mode_test.exe          - Both modes comparison
echo 4. silent_stress_test.exe               - Silent mode (no output)
echo.

echo Choose test to run:
echo [1] ASYNC Mode Test
echo [2] SYNC Mode Test  
echo [3] Comprehensive Comparison Test
echo [4] Silent Mode Test
echo [5] Run All Tests
echo [0] Exit
echo.

set /p choice="Enter your choice (0-5): "

if "%choice%"=="1" goto run_async
if "%choice%"=="2" goto run_sync
if "%choice%"=="3" goto run_comprehensive
if "%choice%"=="4" goto run_silent
if "%choice%"=="5" goto run_all
if "%choice%"=="0" goto end
goto invalid_choice

:run_async
echo.
echo ========================================
echo Running ASYNC Mode Test...
echo ========================================
background_stress_async.exe
goto end

:run_sync
echo.
echo ========================================
echo Running SYNC Mode Test...
echo ========================================
background_stress_async.exe --sync
goto end

:run_comprehensive
echo.
echo ========================================
echo Running Comprehensive Comparison Test...
echo ========================================
comprehensive_mode_test.exe
goto end

:run_silent
echo.
echo ========================================
echo Running Silent Mode Test...
echo ========================================
silent_stress_test.exe
goto end

:run_all
echo.
echo ========================================
echo Running ALL Tests...
echo ========================================
echo.

echo [1/4] Running ASYNC Mode Test...
background_stress_async.exe
echo.

echo [2/4] Running SYNC Mode Test...
background_stress_async.exe --sync
echo.

echo [3/4] Running Comprehensive Comparison Test...
comprehensive_mode_test.exe
echo.

echo [4/4] Running Silent Mode Test...
silent_stress_test.exe
echo.

echo ========================================
echo ALL TESTS COMPLETED!
echo ========================================
echo.
echo Generated reports:
echo - background_stress_results.txt (ASYNC/SYNC results)
echo - comprehensive_mode_report.txt (comparison analysis)
echo - silent_stress_results.txt (silent mode results)
echo.
goto end

:invalid_choice
echo Invalid choice. Please try again.
goto end

:end
echo.
echo Press any key to exit...
pause >nul
