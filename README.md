# SC4 DJEM Fix

SC4 DJEM Fix corrects diagonal jagged edges in steep SimCity 4 terrain. The game divides each terrain cell along one diagonal. Near a sharp height change, the other diagonal can give a smoother surface. This DLL selects the diagonal with the smaller height discontinuity after the game updates terrain normals and cliff textures.

The fix also makes terrain height queries use the corrected diagonal. This keeps prop placement and other height-dependent behavior aligned with the rendered surface.

## Compatibility

The DLL supports **SimCity 4 Deluxe for Windows, version 1.1.641 only**.

## Installation

Copy `SC4DjemFix.dll` and `SC4DjemFix.ini` to a folder in your SimCity 4 Plugins directory. The DLL reads the INI from the same directory as the DLL.

The log file is named `SC4DjemFix.log`. It is written to the `Logs` directory inside the SimCity 4 user data directory. For a standard installation, this is `Documents\SimCity 4\Logs`. The directory is created if it does not exist. If the game does not report a user data directory, the log falls back to the directory holding the DLL.

## Configuration

All values are in the `[SC4DjemFix]` section.

| Setting | Default | Meaning |
| --- | --- | --- |
| `LogLevel` | `info` | Log level: `trace`, `debug`, `info`, `warn`, `error`, `critical`, or `off`. |
| `LogToFile` | `true` | Write `SC4DjemFix.log` when true. |
| `Enabled` | `true` | Enable the terrain diagonal fix. |
| `ClearNonCandidates` | `true` | Remove an old internal flip when a cell no longer meets `MinHeightDelta`. |
| `MatchHeightQueriesToFlippedCells` | `true` | Use the corrected surface for terrain height queries. |
| `MinHeightDelta` | `5.0` | Minimum height range across the four cell corners before the cell is considered for the DJEM fix. |
| `DiagonalHysteresis` | `0.05` | Keep the current diagonal when both choices are almost equal. This prevents repeated switching. |
| `LogEveryNChanges` | `0` | Write a summary after this many cell changes. `0` disables summaries. `1` writes one summary for every terrain-update pass that changes cells. Negative and invalid values use `0`. |

## Build and test

Visual Studio 2022 and a recursive clone are required.

```powershell
cmake --preset vs2022-win32-debug -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
cmake --build --preset vs2022-win32-debug-build
ctest --preset vs2022-win32-debug-test

cmake --preset vs2022-win32-release -DSC4_ENABLE_PLUGIN_DEPLOYMENT=OFF
cmake --build --preset vs2022-win32-release-build
ctest --preset vs2022-win32-release-test
```

## License

SC4 DJEM Fix is licensed under LGPL-3.0-or-later. See `LICENSE.txt` and `THIRD_PARTY_NOTICES.txt`.
