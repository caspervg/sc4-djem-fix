#pragma once

#include "DjemLogic.h"

#include <cstdint>
#include <optional>

class cISTETerrainMap;
template <typename T> class SC4Rect;

namespace Djem {
class TerrainProcessor final {
  public:
    void Configure(const Settings& settings) noexcept;
    void SetActive(bool active) noexcept;

    void Apply(cISTETerrainMap* terrain, const SC4Rect<std::int32_t>* dirtyVertices);

    [[nodiscard]] std::optional<float> TryGetFlippedAltitude(cISTETerrainMap* terrain, float worldX,
                                                             float worldZ) const noexcept;

  private:
    Settings settings_{};
    std::uint64_t cellsExamined_ = 0;
    std::uint64_t candidateCells_ = 0;
    std::uint64_t changedCells_ = 0;
    std::uint64_t changesSinceLastLog_ = 0;
    bool active_ = false;
};
} // namespace Djem
