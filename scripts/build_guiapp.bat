@echo off
set MSDEV=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE
set DSW=C:\jp2_pc\JP2_PC.dsw
set INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include;%INCLUDE%
set LIB=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Lib;%LIB%

echo ===== GUIApp =====
"%MSDEV%" %DSW% /make "GUIApp - Win32 Release"
echo.

copy C:\jp2_pc\Build\Release\GUIApp\GUIApp.exe Z:\GUIApp.exe
echo Done. Exit code: %ERRORLEVEL%
