@echo off
set MSDEV=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE
set DSW=C:\jp2_pc\JP2_PC.dsw
set INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include;%INCLUDE%

echo Copying reg.cpp...
copy Z:\jp2_pc\Source\Lib\Sys\reg.cpp C:\jp2_pc\Source\Lib\Sys\reg.cpp /Y

echo.
echo ===== System.lib =====
"%MSDEV%" %DSW% /make "System - Win32 Release"

echo.
echo ===== trespass.exe =====
"%MSDEV%" %DSW% /make "trespass - Win32 Release"

echo.
copy C:\jp2_pc\Build\Release\trespass\trespass.exe Z:\trespass.exe
echo Done.
