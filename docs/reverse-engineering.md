# Reverse-engineering evidence

This document records the static evidence used by SC4 DJEM Fix. All game addresses below apply only to the Windows x86 `SimCity 4.exe` version 1.1.641.0.

## Hook sites and functions

| Purpose | VA | Static evidence |
| --- | ---: | --- |
| Calculate-normals call site | `0x007498CD` | Bytes `E8 8E A2 FF FF` decode to `0x00743B60`. The caller executes `PUSH ECX`, `MOV ECX, EDI`, then `CALL`, which passes the rectangle on the stack and terrain in `ECX`. |
| Original normals and cliff-texture function | `0x00343B60` | Ghidra identifies `cSTETerrain::CalculateNormalsAndAssignCliffTextures`. It receives `this` in `ECX`, one rectangle pointer on the stack, and ends with `RET 4`. |
| GetAltitude vtable entry | `0x00AB3D4C` | Bytes `60 12 74 00` contain the little-endian pointer `0x00741260`. Constructor and destructor code write vtable base `0x00AB3D18`. The entry is at base plus `0x34`. |
| Original GetAltitude(float, float) | `0x00341260` | Two four-byte stack arguments, `this` in `ECX`, x87 floating-point return, and `RET 8`. The implementation performs 16-unit cell mapping and reads terrain fields and cell records described below. |
| FlipInternalTriangulation | `0x00741180` | `this` in `ECX`, x and z plus a Boolean value on the stack, and `RET 0x0C`. It updates flag `0x1000` at record offset `0x22`. |

The `cISTETerrainMap.h` declaration is consistent with the `GetAltitude(float, float)` ABI. Because its vtable entry is replaced, calling that method virtually from the hook would recurse. The hook therefore uses the validated original target for fallback. `gzcom-dll` does not expose `FlipInternalTriangulation` or `CalculateNormalsAndAssignCliffTextures`, so those verified targets are called directly.

The hook wrappers use the x86 MSVC `__fastcall` bridge form. `ECX` receives the terrain pointer, the unused `EDX` parameter preserves register placement, and original stack arguments remain in their verified order. The saved original function pointers use `__thiscall`.

## Terrain layout and flags

The following values were checked in Windows code at `0x00741260`, `0x00741180`, `0x007411F0`, `0x00740900`, and `0x0074C610`.

| Item | Value | Evidence |
| --- | ---: | --- |
| Cell or vertex count X | `this + 0x28` | Bounds checks in `FlipInternalTriangulation` |
| Cell or vertex count Z | `this + 0x2C` | Bounds checks in `FlipInternalTriangulation` |
| Maximum cell X | `this + 0x30` | Cell clamp in `GetAltitude(float, float)` |
| Maximum cell Z | `this + 0x34` | Cell clamp in `GetAltitude(float, float)` |
| Row stride | `this + 0x38` | Record-index calculation |
| Exclusive world maximum X | `this + 0x50` | `LocationIsInBounds(float, float)` at `0x0074C610` |
| Exclusive world maximum Z | `this + 0x54` | `LocationIsInBounds(float, float)` at `0x0074C610` |
| Cell-record data pointer | `this + 0x6C` | Record reads in altitude and flip functions |
| Record size | `0x24` | Index is multiplied by 9, then by 4 |
| Flags offset | `0x22` | Word access in triangulation functions |
| Cliff-managed flag | `0x0800` | High-byte bit test in `ClearVertexCliffFlipTriangulationFlags` at `0x00740900` |
| Internal flip flag | `0x1000` | OR `0x1000` and AND `0xEFFF` in `0x00741180` |
| External flip flag | `0x2000` | OR `0x2000` and AND `0xDFFF` in `0x007411F0` |

The semantic names for the count fields are inferred from their use and the upstream prototype. Their offsets, widths, comparisons, and role in bounds validation are verified from Windows instructions.

## Why the hooks are placed here

The call at `0x007498CD` is used because the original terrain operation must finish before DJEM examines the affected cells. The wrapper calls `0x00743B60` first and then applies the diagonal choice to the same dirty rectangle.

The vtable entry at `0x00AB3D4C` is used because game callers of `GetAltitude(float, float)` must see the same two triangles as the rendered flipped cell. Cases that are disabled, invalid, outside the terrain, or not flipped go to the original function.