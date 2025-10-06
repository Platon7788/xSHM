@echo off
echo ========================================
echo   XSHM Silent Stress Test
echo   (ABSOLUTELY NO OUTPUT - MAX SPEED)
echo ========================================
echo.

echo Building silent stress test...
bcc64x silent_stress_test.cpp ..\xshm.obj -o silent_stress_test.exe
if %errorlevel% neq 0 (
    echo ERROR: Failed to build silent stress test
    pause
    exit /b 1
)
echo ✅ Silent stress test built successfully
echo.

echo Starting silent stress test...
echo ⚡ ABSOLUTELY NO OUTPUT during test!
echo 📄 Results will be saved to silent_stress_results.txt
echo ⏱️  Test duration: 30 seconds
echo.

silent_stress_test.exe

echo.
echo ========================================
echo Silent stress test completed!
echo ========================================
echo.
echo Results saved to: silent_stress_results.txt
echo.
pause
