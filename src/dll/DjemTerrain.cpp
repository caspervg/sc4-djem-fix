#include "DjemTerrain.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "utils/Logger.h"

#include <SC4Rect.h>
#include <cISTETerrainMap.h>

namespace {
constexpr std::uintptr_t kFlipInternalTriangulation = 0x00741180;
constexpr std::uint32_t kCellCountXOffset = 0x28;
constexpr std::uint32_t kCellCountZOffset = 0x2C;
constexpr std::uint32_t kMaxCellXOffset = 0x30;
constexpr std::uint32_t kMaxCellZOffset = 0x34;
constexpr std::uint32_t kRowStrideOffset = 0x38;
constexpr std::uint32_t kWorldMaxXOffset = 0x50;
constexpr std::uint32_t kWorldMaxZOffset = 0x54;
constexpr std::uint32_t kVertexDataOffset = 0x6C;
constexpr std::uint32_t kCellRecordSize = 0x24;
constexpr std::uint32_t kCellFlagsOffset = 0x22;

using FlipInternalTriangulationFunction = void(__thiscall*)(void*, std::uint32_t, std::uint32_t, bool);
static_assert(sizeof(bool) == 1, "FlipInternalTriangulation uses a one-byte bool value");

template <typename T> [[nodiscard]] T ReadField(const void* base, const std::uint32_t offset) noexcept {
    return *reinterpret_cast<const T*>(reinterpret_cast<std::uintptr_t>(base) + offset);
}

struct TerrainView {
    const std::uint8_t* records = nullptr;
    std::uint32_t cellCountX = 0;
    std::uint32_t cellCountZ = 0;
    std::uint32_t maxCellX = 0;
    std::uint32_t maxCellZ = 0;
    std::uint32_t rowStride = 0;
    float worldMaxX = 0.0F;
    float worldMaxZ = 0.0F;

    [[nodiscard]] const std::uint8_t* CellRecord(const std::uint32_t x, const std::uint32_t z) const noexcept {
        const std::size_t index = static_cast<std::size_t>(z) * rowStride + x;
        return records + index * kCellRecordSize;
    }

    [[nodiscard]] float Height(const std::uint32_t x, const std::uint32_t z) const noexcept {
        return *reinterpret_cast<const float*>(CellRecord(x, z));
    }

    [[nodiscard]] std::uint16_t Flags(const std::uint32_t x, const std::uint32_t z) const noexcept {
        return *reinterpret_cast<const std::uint16_t*>(CellRecord(x, z) + kCellFlagsOffset);
    }

    [[nodiscard]] Djem::CellHeights Heights(const std::uint32_t x, const std::uint32_t z) const noexcept {
        return {Height(x, z), Height(x + 1, z), Height(x, z + 1), Height(x + 1, z + 1)};
    }
};

[[nodiscard]] bool TryGetTerrainView(void* terrain, TerrainView& result) noexcept {
    if (terrain == nullptr) {
        return false;
    }

    TerrainView view;
    view.cellCountX = ReadField<std::uint32_t>(terrain, kCellCountXOffset);
    view.cellCountZ = ReadField<std::uint32_t>(terrain, kCellCountZOffset);
    view.maxCellX = ReadField<std::uint32_t>(terrain, kMaxCellXOffset);
    view.maxCellZ = ReadField<std::uint32_t>(terrain, kMaxCellZOffset);
    view.rowStride = ReadField<std::uint32_t>(terrain, kRowStrideOffset);
    view.worldMaxX = ReadField<float>(terrain, kWorldMaxXOffset);
    view.worldMaxZ = ReadField<float>(terrain, kWorldMaxZOffset);
    view.records = ReadField<const std::uint8_t*>(terrain, kVertexDataOffset);

    if (view.records == nullptr || view.cellCountX < 2 || view.cellCountZ < 2 || view.rowStride < view.cellCountX ||
        view.maxCellX >= view.cellCountX - 1 || view.maxCellZ >= view.cellCountZ - 1) {
        return false;
    }

    const std::uint64_t lastVertexIndex =
        static_cast<std::uint64_t>(view.maxCellZ + 1) * view.rowStride + view.maxCellX + 1;
    if (lastVertexIndex > std::numeric_limits<std::uint32_t>::max() / kCellRecordSize) {
        return false;
    }

    result = view;
    return true;
}
} // namespace

void Djem::TerrainProcessor::Configure(const Settings& settings) noexcept { settings_ = settings; }

void Djem::TerrainProcessor::SetActive(const bool active) noexcept { active_ = active; }

void Djem::TerrainProcessor::Apply(cISTETerrainMap* terrain, const SC4Rect<std::int32_t>* dirtyVertices) {
    if (!active_ || dirtyVertices == nullptr) {
        return;
    }

    TerrainView view;
    if (!TryGetTerrainView(terrain, view)) {
        return;
    }

    const Rect dirtyRect{dirtyVertices->topLeftX, dirtyVertices->topLeftY, dirtyVertices->bottomRightX,
                         dirtyVertices->bottomRightY};
    const CellBounds bounds = ComputeAffectedCellBounds(dirtyRect, view.maxCellX, view.maxCellZ);
    if (!bounds.valid) {
        return;
    }

    const Settings settings = settings_;
    const auto flipInternal = reinterpret_cast<FlipInternalTriangulationFunction>(kFlipInternalTriangulation);
    std::uint64_t examinedThisPass = 0;
    std::uint64_t candidatesThisPass = 0;
    std::uint64_t changedThisPass = 0;

    for (std::uint32_t z = bounds.top;; ++z) {
        for (std::uint32_t x = bounds.left;; ++x) {
            ++examinedThisPass;
            const std::uint16_t flags = view.Flags(x, z);
            const CellDecision decision = EvaluateCell(view.Heights(x, z), flags, settings);
            candidatesThisPass += decision.candidate ? 1U : 0U;

            if (decision.action != CellAction::None) {
                flipInternal(terrain, x, z, decision.action == CellAction::SetFlipped);
                ++changedThisPass;
            }

            if (x == bounds.right) {
                break;
            }
        }

        if (z == bounds.bottom) {
            break;
        }
    }

    cellsExamined_ += examinedThisPass;
    candidateCells_ += candidatesThisPass;
    if (changedThisPass == 0) {
        return;
    }

    changedCells_ += changedThisPass;
    changesSinceLastLog_ += changedThisPass;
    const std::uint32_t interval = settings.logEveryNChanges;
    if (interval > 0 && changesSinceLastLog_ >= interval) {
        LOG_INFO(
            "DJEM: Changed {} cells in this pass from {} candidates. Totals: {} changed, {} candidates, {} examined.",
            changedThisPass, candidatesThisPass, changedCells_, candidateCells_, cellsExamined_);
        changesSinceLastLog_ %= interval;
    }
}

std::optional<float> Djem::TerrainProcessor::TryGetFlippedAltitude(cISTETerrainMap* terrain, const float worldX,
                                                                   const float worldZ) const noexcept {
    if (!active_ || !settings_.matchHeightQueriesToFlippedCells) {
        return std::nullopt;
    }

    TerrainView view;
    WorldCell cell;
    if (!TryGetTerrainView(terrain, view) ||
        !TryMapWorldToCell(worldX, worldZ, view.worldMaxX, view.worldMaxZ, view.maxCellX, view.maxCellZ, cell)) {
        return std::nullopt;
    }

    const std::uint16_t flags = view.Flags(cell.x, cell.z);
    if ((flags & (kInternalFlipFlag | kExternalFlipFlag)) == 0) {
        return std::nullopt;
    }

    return InterpolateFlippedCell(view.Heights(cell.x, cell.z), cell.localX, cell.localZ);
}
