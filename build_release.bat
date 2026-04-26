@echo off
setlocal

set "BUILD_DIR=build\Desktop_Qt_6_10_2_MinGW_64_bit-Release"
set "EXE_NAME=JMCampusNetLinker.exe"
set "SOURCE_EXE=%BUILD_DIR%\%EXE_NAME%"

echo [1/4] Checking built executable...
if not exist "%SOURCE_EXE%" (
    echo Built executable not found: %SOURCE_EXE%
    goto :end
)

echo [2/4] Checking windeployqt...
where windeployqt >nul 2>nul
if errorlevel 1 (
    echo windeployqt not found. Open a Qt command prompt or add Qt\bin to PATH.
    goto :end
)

echo [3/4] Copying exe to dist...
if not exist dist mkdir dist
copy /Y "%SOURCE_EXE%" "dist\%EXE_NAME%"
if errorlevel 1 (
    echo Failed to copy exe.
    goto :end
)

echo [4/4] Running windeployqt. This may take some time...
windeployqt --release --no-translations "dist\%EXE_NAME%"
if errorlevel 1 (
    echo windeployqt failed.
    goto :end
)

echo Package complete. Use Inno Setup to package the dist\ directory.

:end
echo.
pause
