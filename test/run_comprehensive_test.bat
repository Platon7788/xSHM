@echo off
echo ========================================
echo    XSHM Comprehensive Test Runner
echo ========================================
echo.

echo Starting comprehensive analysis test...
echo This test will run for 60 seconds and include:
echo   - Normal load test (30 seconds)
echo   - Stress test (30 seconds)
echo.
echo Results will be saved to:
echo   - detailed_analysis_report.txt
echo   - summary_analysis_report.txt
echo.

comprehensive_analysis.exe

echo.
echo Test completed!
echo Check the report files for detailed results.
pause
