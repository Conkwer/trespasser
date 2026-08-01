@echo off
set MSDEV=C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE
set DSW=C:\jp2_pc\JP2_PC.dsw
set INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include;%INCLUDE%

echo Copying updated files...
xcopy Z:\jp2_pc\Source\Trespass\*   C:\jp2_pc\Source\Trespass\   /Y > NUL
xcopy Z:\jp2_pc\Source\Lib\File\*    C:\jp2_pc\Source\Lib\File\    /Y > NUL
xcopy Z:\jp2_pc\Source\Lib\Sys\*     C:\jp2_pc\Source\Lib\Sys\     /Y > NUL
echo Done.

echo ===== trespass.exe =====
"%MSDEV%" %DSW% /make "trespass - Win32 Release"
echo.

copy C:\jp2_pc\Build\Release\trespass\trespass.exe Z:\trespass.exe
echo Copied to Z:
echo Done.
