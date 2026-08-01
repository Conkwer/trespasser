# Trespasser Build Guide

## Restoring the Toolchain

The toolchain is manual but reproducible:
1. Install **Windows XP SP3 (32-bit)** VM
2. Install **Visual Studio 6.0** + **SP6**
3. Install **FreeSSHd** for remote access
4. Configure host-only network: `192.168.56.101`
5. Map shared folder to `Z:\` — copy source to `C:\jp2_pc`

Build scripts in `scripts/` automate the rest. SSH config on the Linux side:
```
Host winxp
  HostName 192.168.56.101
  User root
  HostKeyAlgorithms +ssh-rsa
  KexAlgorithms +diffie-hellman-group1-sha1
  MACs +hmac-sha1
  Ciphers aes128-cbc
  StrictHostKeyChecking no
```

## Environment

- **Windows XP SP3** (32-bit) VM with Visual Studio 6.0 + SP6
- DirectX headers and libs are **bundled** in the source (`Inc/DirectX/`, `lib/DirectX/`) — no DX SDK required
- Network: Host-only adapter at `192.168.56.101`, SSH via FreeSSHd

## VS6 Configuration

Add these to **Tools → Options → Directories**:

| Type | Path |
|---|---|
| Include | `C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include` |
| Lib | `C:\Program Files\Microsoft Visual Studio\VC98\MFC\Lib` |

Or set environment variables before building:
```
set INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Include;%INCLUDE%
set LIB=C:\Program Files\Microsoft Visual Studio\VC98\MFC\Lib;%LIB%
```

## Build

Source must be at `C:\jp2_pc` (hardcoded paths in `.dsp` files).

```bat
"C:\Program Files\Microsoft Visual Studio\Common\MSDev98\Bin\MSDEV.EXE" C:\jp2_pc\JP2_PC.dsw /make "trespass - Win32 Release"
```

## Source Fixes Applied

### File.lib — STL compatibility (4 files)
`src/Source/Lib/File/Image.hpp`, `Image.cpp`, `Section.hpp`, `Section.cpp`

The bundled HP/SGI STL in `Inc/STL/` uses single-template-argument `vector<T>` in the global namespace. VS6's STL uses `std::vector<T, Alloc>`. The code was written for VS6's STL but headers didn't include `<vector>`.

**Fixed by:**
- Adding `#include <vector>` to headers that use `vector<T, Alloc>`
- Adding typedefs for each vector type (VS6 cannot resolve `vector<T>::iterator` as a scope qualifier)
- Replacing `vector<T>::iterator` in .cpp files with raw pointer types (VS6's STL uses `T*` as iterator)

### Processor detection bypass (1 file)
`src/Source/Trespass/main.cpp`

1998 CPUID code in `processor.dll` cannot identify modern CPUs (Ryzen family 0x17 is out-of-bounds for the AMD cycle-count array). The Pentium/PentiumPro build doesn't need CPU detection.

**Fixed by:** Early-returning `true` from `VerifyProcessor()` when `TARGET_PROCESSOR` is `PROCESSOR_PENTIUM` or `PROCESSOR_PENTIUMPRO`.

### ESC menu instant-close (2 files)
`src/Source/Trespass/uidlgs.h`, `uidlgs.cpp`

`CGameWnd::OnKey` opens the menu on ESC key-down. `CUIDlg::OnKey` closes dialogs on ESC key-up. The same physical press triggers both.

**Fixed by:** `CInGameOptionsWnd` constructor checks if ESC is physically held when the dialog opens. If so, the first ESC key-up is consumed. Subsequent ESC presses close normally.

## Build Results

| Project | Status |
|---|---|
| trespass.exe | PASS |
| GUIApp.exe | PASS |
| AI.lib | PASS |
| Audio.lib | PASS |
| EntityDBase.lib | PASS |
| File.lib | PASS (fixed) |
| Game.lib | PASS |
| GeomDBase.lib | PASS |
| Loader.lib | PASS |
| Math.lib | PASS |
| Physics.lib | PASS |
| Render3D.lib | PASS |
| ScreenRenderDWI.lib | PASS (assembly compiled) |
| Std.lib | PASS |
| System.lib | PASS |
| View.lib | PASS |
| WinShell.lib | PASS |
| Processor.dll | PASS |
| GroffBuild.exe | Not linked (missing globals) |

## Known Limitations

- **v1.0 source only** — matches v1.0 retail data, not v1.1 patched data
- **DirectX 6 renderer** — use [dgVoodoo2](https://dege.freeweb.hu/dgVoodoo2/dgVoodoo2/) to wrap DX6→DX12 with resolution scaling. DX9 migration not planned (dgVoodoo2 handles this)
- **Audio** — DirectSound3D via [DSOAL](https://www.nexusmods.com/oblivion/mods/48872) wrapper (DS→OpenAL). Native OpenAL support planned
- **Input** — Win32 keyboard + DirectInput. XInput controller support possible via DirectX SDK without renderer changes
- **Registry-free config** — `trespass.ini` replaced Windows registry
- **16-bit color mode required** — GUIApp needs 16-bit display. Win11: Compatibility → Reduced color mode

### INI config instead of registry (1 file)
`src/Source/Lib/Sys/reg.cpp`

Replaced all Windows registry calls with `GetPrivateProfileString`/`WritePrivateProfileString` targeting `.\trespass.ini` in a flat `[Settings]` section. On first run, the game creates the INI with defaults from `RegInit.hpp`. Binary data (KeyMap, GUIDs) is stored as hex strings.
