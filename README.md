# 🦖 Trespasser-Plus

Trespasser in all it C++98 glory. You should be insane to compile it.  
This is a preservation of the Trespasser engine source code, compiled and maintained with fixes for "modern" systems.

**Game assets are not included.** You must own the original CD (v1.0 retail, 1998) and copy the data files to use the compiled binary.

## Quick Start

Pre-compiled binaries are in [`dist/`](dist/):
- `trespass.exe` — the game
- `GUIApp.exe` — developer debug shell (requires 16-bit color mode)

To run: copy `trespass.exe` into your installed game directory (where the .GRF data files are), or copy the data files alongside the binary.

## Build

The engine compiles on **Windows XP SP3 + Visual Studio 6.0 SP6**. No DirectX SDK is required — all headers and libs are bundled.

See [`BUILD.md`](BUILD.md) for the full toolchain setup, build commands, and details on each source fix.

## Fixes Applied

1. **File.lib STL compatibility** — VS6 `vector<T>::iterator` workaround
2. **Processor detection bypass** — modern CPUs are unrecognizable to 1998 CPUID
3. **ESC menu instant-close** — dialog no longer closes on the same keypress that opened it
4. **MFC paths** — resource compiler include/lib configuration

## Status

| Area | State |
|---|---|
| Source compilation | 17/19 projects, 0 errors |
| DX6 renderer | Working (software + hardware) |
| Audio | DirectSound3D with EAX/A3D (native) |
| Modern audio | OpenAL replacement pending |
| Config storage | Windows registry (INI replacement pending) |

## Solution Structure

The workspace contains 23 sub-projects (~300K lines of C++ across 1241 files).

> Source: Fabien Sanglard, [Trespasser Source Code Review](http://fabiensanglard.net/trespasser) (2014).

| Project | Generates | Description |
|---|---|---|
| AI | AI.lib | Artificial intelligence subsystem (mostly disabled) |
| AI Test | JP2_PC.exe | Standalone AI testing with graphics |
| Audio | Audio.lib | Real-time Foley sound system |
| Bug | Bugs.exe | Compiler error isolation tool |
| CollisionEditor | CollisionEditor.exe | Audio engine test tool |
| EntityDBase | EntityDBase.lib | Game object class definitions |
| File | File.lib | GROFF archive I/O abstraction |
| File Test | File.exe | File/image abstraction tests |
| Game | Game.lib | Glue, triggers, Player, Gun classes |
| GeomDBase | GeomDBase.lib | 3D geometry for all EntityDBase objects |
| GroffBuild | GroffBuild.exe | Asset packer — bundles assets into GROFF archives |
| GroffExp | GroffExp.dle | 3DS Max plugin — exports data to GROFF format |
| GUIApp | GUIApp.exe | Runtime debug shell (change values at runtime) |
| Loader | Loader.lib | GROFF asset loader |
| Math | Math.lib | Math library (fInvSqrt via lookup table + Newton-Raphson) |
| MathTest | MathTest.exe | Math routine speed tests |
| Physics | Physics.lib | Pelvis-heavy, penalty-force physics engine |
| PhysicsTest | PhysicsTest.exe | Physics sandbox |
| PipeLineTest | PipeLineTest.exe | Render pipeline testbed |
| Processor | Processor.dll | CPUID-based CPU detection and speed measurement |
| QuantizerTool | QuantizerTool.exe | Aborted — does nothing |
| Render3D | Render3D.lib | Hybrid software/Direct3D renderer |
| ScreenRenderDWI | ScreenRenderDWI.lib | Hand-tuned ASM for P5/P6/K6-3D scanline rendering |
| Std | Std.lib | STL extensions |
| System | System.lib | Scheduler, virtual memory, threads, self-modifying code support |
| trespass | trespass.exe | The game |
| View | View.lib | Raster-to-window: blitter, DirectDraw, D3D, palette viewers |
| WaveTest | WaveTest.exe | Wave modeling test shell |
| WinShell | WinShell.lib | Win32 window creation and management |

## Related

- [TresCom Forum](https://www.trescomforum.org/) — active modding community
- [OpenTrespasser](https://github.com/OpenTrespasser/JurassicParkTrespasser) — CMake modernization attempt (abandoned)
- [Trespasser CE](https://www.trescomforum.org/viewforum.php?f=6) — closed-source DX9/OpenAL fork (2014)

## Disclaimer

The source code surfaced within the community after many years of preservation efforts. This repository exists for **educational and archival purposes only**. No game assets, copyrighted materials, or proprietary data are included. This is not a commercial project. If you hold rights to any content here and wish it removed, open an issue.
