
#Original Trespasser, vanilla source code v1.0 (1998):
/mnt/data/projects/trespasser/original-source-code/jp2_pc
When DreamWorks was sold to EA, their servers were repurposed. The v1.1 patch source code
is likely lost forever — only v1.0 survived via community preservation. EA may not even
have it anymore.

#the fork from fans named "OpenTrespasser", not completed (2020?)
/mnt/data/projects/trespasser/OpenTrespasser/JurassicParkTrespasser-master
They tried to modernize it 8 years ago and make cmake compatible but not succeded. I heard it cannot be compiled or even if it can it have a ton of problems

#game manual, not important
/mnt/data/projects/trespasser/game-manual
have recognized version Trespasser_Jurassic_Park_text.pdf.txt
The only useful info can be the official system requirement:

- Windows'95 or Windows'98
- Pentium 166MHZ (for 320x240@10fps must be, since for 1080p it require something like Ryzen. The game is purely optimized and single core. In software it works faster on modern PCs then in hardware mode - added by me)
- 32 MB RAM (unlikely, if max settings - added by me)
- 120MB Free Disk Space (must be if data loaded from CD, since it can took more then 1Gb if properly installed - added by me)
- DirectX 6.0 (Included)
- 100% Windows Compatible Mouse
- 100% Windows Compatible Sound Card (the original retail game engine natively supported both EAX 1.0 and Aureal A3D 1.0. It utilized Microsoft DirectSound3D as its primary software middleware layer, allowing it to bridge out directly to proprietary sound card expansion sets - added by me).
- 4x CD-ROM
- 100% Windows Compatible IMB Video Саrd (supports software DirectDraw mode and DirectX6 accelerated mode - added by me)
Supports most major 3D acceleration cards (of that era, obviously - added by me). 

  Recommended
- AMD K6-2 OR Pentium II 266MHz Processor (the game usually had 2 binaries, one optimized for AMD and have assembly code, but it works on modern Intel CPUs anyways despite it says that's for AMD - added by me).
- 64MB RAM
- AGP2X 3D Accelerator Card


#builds

#Clean version with minimal needed to run the level:
/mnt/data/projects/trespasser/builds/Trespasser-Minimal
not patched for modern systems

#Minimal version that is patched with ATX patch
/mnt/data/projects/trespasser/builds/Trespasser-ATX
Also it use DDraw.dll -> DirectX12 wrapper (dgVoodoo2); D3DImm.dll (DirectX6) -> DirectX12 wrapper (dgVoodoo2); ATXconfig.ini - config for dgVoodoo2

#this is the full version, with patches for modern system:
/mnt/data/projects/trespasser/builds/TrespasserCE
Have the same what /mnt/data/projects/trespasser/builds/Trespasser-ATX have but also dsound.dll, dsoal-aldrv.dll (DirectSound -> OpenAL)
Also have tpassp6.exe + tpass.ini, this is upgraded closed-source custom implementation of trespass.exe. The author of this for is dissapeared not leaving the source code. He upgraded the engine: added OpenAL, DirectX9, shader water, portable config instead of registry tpass.ini. This all took for him about ~3 month in 2014 according to lore and he burned out. In his version DirectDraw mode is buggy and do not have a water, as far as I remember. So it only playable in DirectX9 but textures can be configured to be shown with nearest neighbor scaling and it match DirectDraw mode visually in this way. On modern GPU DirectX9 struggle to run anyways and need the same sort of wrapper as DirectX6. As for wine I dunno but the wine usually treat dgVoodoo2 wery well and lot of the game works good if any DirectX -> DirectX12 -> Wine (Proton?) -> Vulkan whatever (not for hearderless env tho). The game is not hearderless tho, but can have a logging, I dunno.

#code overwiew by OpenTrespasser lead:
/mnt/data/projects/trespasser/source-code-overview.md

#info from web search, can be not reliable:
/mnt/data/projects/trespasser/info.md

#our fork
GitHub: https://github.com/Conkwer/trespasser (branch: dev)
Source moved to src/ subfolder. Root has BUILD.md with compile instructions.

#WinXP build VM
VirtualBox VM, WinXP SP3 32-bit, VS6 SP6. Host-only network at 192.168.56.101.
SSH via FreeSSHd: `ssh winxp` (key in ~/.ssh/config). Shared folder at Z: = /mnt/shared.
Build trigger: `ssh winxp "cmd.exe /c Z:\\rebuild_trespass.bat"` or similar batch files.
Source must be at C:\jp2_pc (hardcoded paths in .dsp files).
No DX SDK needed — DX6 headers/libs bundled in Inc/DirectX/ and lib/DirectX/.

#compilation fixes applied (see BUILD.md for details)
1. File.lib STL — added missing #include <vector>, typedefs for VS6 vector<T>::iterator bug
2. Processor detection bypass — Pentium/P6 builds skip CPUID check (modern CPUs unrecognized)
3. ESC menu instant-close — CInGameOptionsWnd suppresses first ESC key-up from menu-open press
4. MFC include/lib paths — needed for trespass.exe and GUIApp.exe resource compilation

Build: 17/19 projects compile. trespass.exe + GUIApp.exe = 0 errors. GroffBuild = link fail (missing globals, non-essential).

#build targets
We build "trespass - Win32 Release" = TARGET_PROCESSOR=PROCESSOR_PENTIUMPRO (P6/PII).
This is the Intel-optimized binary. Modern AMD Ryzen runs it fine.
K6 target uses 3DNow! instructions which Ryzen removed — would crash.

#known issues
- Source is v1.0 retail. v1.1 patch data files cause "Cannot instance" errors (class table mismatch).
- GUIApp is a developer debug shell, not a level editor. Crashes with v1.1 data. Community replaced it with TresEd + GeomAdd.
- Registry-based config still present (HKLM\Software\DreamWorks Interactive\Trespasser). INI replacement planned.
- 16-bit color mode required for GUIApp. Win11: Compatibility → Reduced color mode works.
- WinXP SP3 blocks ICMP ping by default — ARP works, SSH works.