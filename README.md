# SC4 DJEM Fix

SC4 DJEM Fix corrects diagonal jagged edges in steep SimCity 4 terrain. The game divides each terrain cell along one diagonal. Near a sharp height change, the other diagonal can give a smoother surface. This DLL selects the diagonal with the smaller height discontinuity after the game updates terrain normals and cliff textures.

The fix also makes terrain height queries use the corrected diagonal. This keeps prop placement and other height-dependent behavior aligned with the rendered surface.

## Compatibility

The DLL supports **SimCity 4 Deluxe for Windows, version 1.1.641 only**. It does not claim support for 1.1.610, 1.1.638, 1.1.640, or any other executable.

The DLL checks the executable version and the original instruction targets before it writes game memory. An unsupported or modified executable fails closed. No hook is installed when a check fails.

## Installation

Copy `SC4DjemFix.dll` and `SC4DjemFix.ini` to a folder in your SimCity 4 Plugins directory. The DLL reads the INI from the same directory as the DLL.

The log file is named `SC4DjemFix.log`. It is written to the parent directory of the user Plugins directory. For a standard installation, this is the `Documents\SimCity 4` directory.

## Configuration

All values are in the `[SC4DjemFix]` section.

| Setting | Default | Meaning |
| --- | --- | --- |
| `LogLevel` | `info` | Log level: `trace`, `debug`, `info`, `warn`, `error`, `critical`, or `off`. |
| `LogToFile` | `true` | Write `SC4DjemFix.log` when true. |
| `Enabled` | `true` | Enable the terrain diagonal fix. |
| `ClearNonCandidates` | `true` | Remove an old internal flip when a cell no longer meets `MinHeightDelta`. |
| `MatchHeightQueriesToFlippedCells` | `true` | Use the corrected surface for terrain height queries. |
| `MinHeightDelta` | `12.0` | Minimum height range across the four cell corners before the cell is considered. |
| `DiagonalHysteresis` | `0.05` | Keep the current diagonal when both choices are almost equal. This prevents repeated switching. |
| `LogEveryNChanges` | `0` | Write a summary after this many cell changes. `0` disables summaries. `1` writes one summary for every terrain-update pass that changes cells. Negative and invalid values use `0`. |

Invalid Boolean, numeric, non-finite, negative, or overflowing values produce a warning and use a safe default.

## How it works

The DLL first calls the game's original normals and cliff-texture function. It then processes only the cells affected by that function's dirty vertex rectangle. It does not change cells managed by the game's cliff system.

Hook installation uses the patching and version-detection code from `sc4-dll-utilities`. The terrain calculation, terrain-memory access, and hook lifecycle are in separate components. The hot cell loop does not allocate memory, write logs, read files, perform virtual calls, or take locks.

`cISTETerrainMap::GetAltitude(float, float)` is declared by `gzcom-dll`, and that declaration is used for the terrain type and ABI. The DLL must hook its vtable entry so existing game callers see corrected heights. The hook cannot call the same virtual method for fallback because that would recurse. It calls the original target only after the vtable target was validated.

## Build and test

Visual Studio 2022 and a recursive clone are required. These commands keep automatic Plugins deployment off:

```powershell
cmake --preset vs2022-win32-debug -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
cmake --build --preset vs2022-win32-debug-build
ctest --preset vs2022-win32-debug-test

cmake --preset vs2022-win32-release -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
cmake --build --preset vs2022-win32-release-build
ctest --preset vs2022-win32-release-test
```

SC4 is a 32-bit process. CMake rejects a 64-bit compiler. In CLion, select an x86 MSVC toolchain or use the Visual Studio Win32 presets. Selecting Ninja with the default x64 MSVC toolchain cannot link the required x86 dependencies.

## Verification status

The Windows 1.1.641 executable was checked statically in Ghidra. The call target, vtable entry, original functions, calling conventions, object offsets, record size, and terrain flags are recorded in [docs/reverse-engineering.md](docs/reverse-engineering.md).

Automated tests cover diagonal selection, thresholds, hysteresis, stale flips, cliff exclusion, dirty-area bounds, altitude interpolation, invalid coordinates, settings, and relative-call guard calculations.

No game process is launched during automated verification. A release should still receive an in-game smoke test on Windows 1.1.641:

1. Confirm that the log reports successful hook installation.
2. Raise and lower steep terrain and confirm that diagonal spikes are removed.
3. Place props on corrected cells and confirm that their height matches the rendered terrain.
4. Disable each optional behavior in the INI and confirm the documented result.
5. Exit the game normally and confirm that no shutdown error is logged.

## Provenance and license

The behavior was adapted from the `re/djem` branch of [caspervg/sc4-render-services](https://github.com/caspervg/sc4-render-services/tree/re/djem), resolved at commit `c146049fd5ccab2c30d4156a731a06f2a8160285`. That sample was used as a behavioral prototype. Its patching and version-detection helpers were not copied.

SC4 DJEM Fix is licensed under LGPL-3.0-or-later. See `LICENSE.txt` and `THIRD_PARTY_NOTICES.txt`.
