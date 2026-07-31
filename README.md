# Jurassic Park: Trespasser — Source Port

This is the original leaked Trespasser source code (v1.0 retail, 1998), compiled and gradually fixed for modern systems.

## What Works

The game compiles on **Windows XP SP3 + Visual Studio 6.0 SP6** with zero errors. No DirectX SDK is needed — all headers and libs are bundled in the source.

## Quick Start

Pre-compiled binaries are in [`dist/`](dist/):
- `trespass.exe` — the game
- `GUIApp.exe` — developer debug shell (requires 16-bit color mode)

## Build

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
| OpenTrespasser comparison | Their CMake modernization is abandoned; we build with the original toolchain |

## Requirements

- v1.0 retail game data (v1.1 patched data causes version mismatch)
- Windows XP or newer (Win11 + Compatibility mode for 16-bit color)

## Related

- [TresCom Forum](https://www.trescomforum.org/) — active modding community
- [OpenTrespasser](https://github.com/OpenTrespasser/JurassicParkTrespasser) — abandoned CMake modernization attempt
- [Trespasser CE](https://www.trescomforum.org/viewforum.php?f=6) — closed-source DX9/OpenAL fork (author disappeared in 2014)

## License

The source code was shared unofficially by DreamWorks Interactive developers. No official license exists. This repository is for preservation and research.
