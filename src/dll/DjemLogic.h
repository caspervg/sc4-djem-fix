#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace Djem {
inline constexpr std::uint16_t kCliffManagedFlag = 0x0800;
inline constexpr std::uint16_t kInternalFlipFlag = 0x1000;
inline constexpr std::uint16_t kExternalFlipFlag = 0x2000;
inline constexpr float kCellSize = 16.0F;

struct Settings {
    bool enabled = true;
    bool clearNonCandidates = true;
    bool matchHeightQueriesToFlippedCells = true;
    float minHeightDelta = 5.0F;
    float diagonalHysteresis = 0.05F;
    std::uint32_t logEveryNChanges = 0;
};

struct Rect {
    std::int32_t left;
    std::int32_t top;
    std::int32_t right;
    std::int32_t bottom;
};

struct CellBounds {
    std::uint32_t left = 0;
    std::uint32_t top = 0;
    std::uint32_t right = 0;
    std::uint32_t bottom = 0;
    bool valid = false;
};

struct CellHeights {
    float h00;
    float h10;
    float h01;
    float h11;
};

enum class CellAction : std::uint8_t { None, SetNormal, SetFlipped };

struct CellDecision {
    CellAction action = CellAction::None;
    bool candidate = false;
};

struct WorldCell {
    std::uint32_t x = 0;
    std::uint32_t z = 0;
    float localX = 0.0F;
    float localZ = 0.0F;
};

[[nodiscard]] CellDecision EvaluateCell(const CellHeights& heights, std::uint16_t flags,
                                        const Settings& settings) noexcept;

[[nodiscard]] CellBounds ComputeAffectedCellBounds(const Rect& dirtyVertices, std::uint32_t maxCellX,
                                                   std::uint32_t maxCellZ) noexcept;

[[nodiscard]] bool TryMapWorldToCell(float worldX, float worldZ, float worldMaxX, float worldMaxZ,
                                     std::uint32_t maxCellX, std::uint32_t maxCellZ, WorldCell& result) noexcept;

[[nodiscard]] float InterpolateFlippedCell(const CellHeights& heights, float localX, float localZ) noexcept;

using RelativeCallBytes = std::array<std::uint8_t, 5>;

[[nodiscard]] std::optional<std::uintptr_t> DecodeRelativeCallTarget(std::uintptr_t instructionAddress,
                                                                     const RelativeCallBytes& bytes) noexcept;

[[nodiscard]] std::optional<RelativeCallBytes> EncodeRelativeCall(std::uintptr_t instructionAddress,
                                                                  std::uintptr_t targetAddress) noexcept;
} // namespace Djem
