@echo off
set MSDEV=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE
set DSW=C:\jp2_pc\JP2_PC.dsw
set INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include;%INCLUDE%

copy Z:\jp2_pc\Source\Lib\Control\Control.cpp C:\jp2_pc\Source\Lib\Control\Control.cpp /Y > NUL
copy Z:\jp2_pc\Source\Lib\Sys\reg.cpp C:\jp2_pc\Source\Lib\Sys\reg.cpp /Y > NUL
copy Z:\jp2_pc\Source\Trespass\main.cpp C:\jp2_pc\Source\Trespass\main.cpp /Y > NUL

echo ===== System.lib (forced) =====
"%MSDEV%" %DSW% /make "System - Win32 Release" /rebuild

echo ===== trespass.exe =====
"%MSDEV%" %DSW% /make "trespass - Win32 Release"

copy C:\jp2_pc\Build\Release\trespass\trespass.exe Z:\trespass.exe
echo Done.
