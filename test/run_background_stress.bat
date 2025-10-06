@echo off
echo ========================================
echo   XSHM Background Stress Test
echo   (NO CONSOLE OUTPUT - MAXIMUM SPEED)
echo ========================================
echo.

echo Building background stress test...
bcc64x background_stress_test.cpp ..\xshm.obj -o background_stress_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build background stress test
    pause
    exit /b 1
)
echo ✅ Background stress test built successfully
echo.

echo Starting background stress test...
echo ⚡ NO CONSOLE OUTPUT during test for maximum speed!
echo 📄 Results will be saved to background_stress_results.txt
echo ⏱️  Test duration: 30 seconds
echo.

background_stress_test.exe

echo.
echo ========================================
echo Background stress test completed!
echo ========================================
echo.
echo Results saved to: background_stress_results.txt
echo.
pause
