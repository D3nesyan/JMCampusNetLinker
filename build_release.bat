@echo off
setlocal

set "EXE_NAME=JMCampusNetLinker.exe"

:: Detect build directory
set "BUILD_DIR="
for %%d in (
    "build\Desktop_Qt_6_10_2_MinGW_64_bit-Release"
    "build"
) do (
    if exist "%%~d\CMakeCache.txt" (
        set "BUILD_DIR=%%~d"
    )
)

if "%BUILD_DIR%"=="" (
    echo No existing CMake build found. Configuring...
    cmake -B build -G "MinGW Makefiles"
    if errorlevel 1 (
        echo CMake configure failed. Set CMAKE_PREFIX_PATH to your Qt install.
        goto :end
    )
    set "BUILD_DIR=build"
)

echo Build directory: %BUILD_DIR%

echo [1/7] Building project...
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 (
    echo Build failed.
    goto :end
)

set "SOURCE_EXE=%BUILD_DIR%\%EXE_NAME%"

echo [2/7] Checking built executable...
if not exist "%SOURCE_EXE%" (
    echo Built executable not found: %SOURCE_EXE%
    goto :end
)

echo [3/7] Checking windeployqt...
where windeployqt >nul 2>nul
if errorlevel 1 (
    echo windeployqt not found. Open a Qt command prompt or add Qt\bin to PATH.
    goto :end
)

echo [4/7] Preparing dist directory...
if exist dist rmdir /S /Q dist
mkdir dist

echo [5/7] Copying exe and assets to dist...
copy /Y "%BUILD_DIR%\%EXE_NAME%" "dist\%EXE_NAME%" >nul
if exist "%BUILD_DIR%\theme.qss" copy /Y "%BUILD_DIR%\theme.qss" "dist\theme.qss" >nul
if exist "%BUILD_DIR%\fonts" xcopy /E /Y /Q "%BUILD_DIR%\fonts" "dist\fonts\" >nul

echo [6/7] Running windeployqt. This may take some time...
windeployqt --release --no-translations "dist\%EXE_NAME%"
if errorlevel 1 (
    echo windeployqt failed.
    goto :end
)

echo [7/8] Creating portable package...
if not exist Output mkdir Output
powershell -NoProfile -Command "Compress-Archive -Path 'dist\*' -DestinationPath 'Output\JMCampusNetLinker_Portable_x64.zip' -Force"
if errorlevel 1 (
    echo Portable package failed.
) else (
    echo Portable package created: Output\JMCampusNetLinker_Portable_x64.zip
)

echo [8/8] Creating installer with Inno Setup...
set "ISCC="
for %%d in ("C:\Program Files (x86)" "C:\Program Files" "D:\Program Files (x86)" "D:\Program Files" "E:\Program Files (x86)" "E:\Program Files") do (
    if exist "%%~d\Inno Setup 6\ISCC.exe" (
        set "ISCC=%%~d\Inno Setup 6\ISCC.exe"
    )
)
if not "%ISCC%"=="" (
    "%ISCC%" installer.iss
    if errorlevel 1 (
        echo Inno Setup failed.
    ) else (
        echo Installer created: Output\JMCampusNetLinker_Setup_x64.exe
    )
) else (
    echo Inno Setup 6 not found. Skipping installer generation.
    echo dist\ directory is ready for manual packaging.
)

:end
echo.
pause
