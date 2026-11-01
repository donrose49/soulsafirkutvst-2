@echo off
REM Necessite : Visual Studio 2022 (charge de travail "Developpement Desktop en C++"), CMake et Git
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
echo.
echo Le plugin se trouve dans build\SoulKut_artefacts\Release\VST3\
echo et a ete copie dans C:\Program Files\Common Files\VST3\
pause
