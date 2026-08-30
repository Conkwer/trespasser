# Changelog

All notable changes to **Trespasser-Plus** (the hardcore gameplay branch + the
experimental directx9 render branch).

The format is Keep-a-Changelog-lite. The git history holds the per-commit detail;
this file is the curated, feature-level view.

## [Unreleased] — directx9 render (experimental, in progress)

### New render backend (branch `directx9`)
- **D3D9 device layer over the original D3D6 pipeline** — the game runs natively on
  modern systems (Win11 + dgVoodoo2, Wine) with no D3D6/DDraw emulation. The renderer
  itself is unchanged: same batches, draw order, state machine, software fallback.
- **Terrain texture pages fixed** — the pages were falling back to CPU-only rasters
  (no D3D9 texture existed at all), which caused the flat-white terrain. They are
  D3D9 managed-texture twins now; the terrain/grass renders textured.
- State hygiene: COLOROP/ALPHAOP restored to the D3D9 defaults, fog colour sent per
  scene, Z-clear to the fog colour, ZFUNC leak removed (a leftover D3DCMP_ALWAYS in
  the query setup), batch preraster Z-bias (flat fills sit behind the textured polys).
- Readback cost cut: the render-to-software handoff only triggers on real hardware
  draws (draw-latch) — 42 readbacks/frame → 3.

### Known issues (WIP)
- Ocean/water still flat white; rocks/objects render flat (texture present, draw-side
  issue) — the per-poly terrain path (cliffs) is textured.
- FPS lower than the D3D6 path under software GL (~10-13 fps in level under Wine).

### Tooling
- **Local build**: MSVC 6 runs under Wine — `build_local.py` compiles + links the
  whole game on Linux, no WinXP VM needed (see `build.md`).

## [1.0.123-beta3] — 2026-08-28

### Gameplay / modding
- **Hardcore difficulty mode** (`Difficulty=3`): dino revive system (escalating
  10/15/20s respawn timers, 50% HP, upright physics reset, scaled 1.5x/2x/2.5x damage
  per revive), kill-heal (+20 HP per dino kill), water-only HP regen (20 HP/s),
  instant death, +50% damage taken, max active dinos 4→8, AI 2x faster (20 Hz),
  wake distance 3x, damage causes anger.
- **Ammo randomization on weapon pickup** (25–150%, persistent).
- **Jump/physics stability**: 150ms jump input buffer, NaN bailouts in constraint
  loops, velocity cap 13→20 m/s.
- **Cheats**: `JUMP <mult>`, `FAST`, `FASTER` (independent state, no corruption).
- **Arcade jump feel**: relaxed jump gate (~57°) + latch-until-landing
  (`legacyjump=1` / `legacyjumpgate=1` restore vanilla).

### Audio
- **Background music**: DirectSound player, clean-room CRI ADX decoder, IMA ADPCM,
  M3U playlists, track-by-track playback.
- **Override system** (`override\`): TPA/loose-sound/SRT-subtitle/GRF/BGM overrides,
  per-language files, tpz stored-ZIP archives, two-line caption stacking, UTF-8 font.

### Graphics
- `DrawDistance`, `ObjectClarityDrawDistance`, ATX-style fog override
  (`OverrideGameFogValues` + `FogHalfForcedValue` + `FogPowerForcedValue`).

### Bug fixes
- CD-loop dialog, swap-file crash (Terminal Error 1004), empty-path loader OOB read
  (all 6 sites), 64-bit disk-free checks, logging consolidation (`Debug=1`).

## [1.0.118] — 2026-08-xx
- INI-file config (registry replaced), keymap defaults, GUIApp rebuild, renamed to
  Trespasser-Plus (was Trespasser-Reloaded).

## [1.0.117] — 2026-xx-xx
- (early port/modding work; see git history `v1.0.1..v1.0.117`)

## [1.0.1] — 2026-xx-xx
- Original port baseline.
