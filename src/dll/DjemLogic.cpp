#include "DjemLogic.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>

namespace Djem
{
	CellDecision EvaluateCell(
		const CellHeights& heights,
		const std::uint16_t flags,
		const Settings& settings) noexcept
	{
		if ((flags & kCliffManagedFlag) != 0)
		{
			return {};
		}

		const auto [minHeight, maxHeight] = std::minmax(
			{ heights.h00, heights.h10, heights.h01, heights.h11 });
		const bool currentlyFlipped = (flags & kInternalFlipFlag) != 0;

		if (maxHeight - minHeight < settings.minHeightDelta)
		{
			return {
				settings.clearNonCandidates && currentlyFlipped ? CellAction::SetNormal : CellAction::None,
				false
			};
		}

		const float normalDiagonalDelta = std::fabs(heights.h00 - heights.h11);
		const float flippedDiagonalDelta = std::fabs(heights.h10 - heights.h01);
		bool shouldFlip = currentlyFlipped;

		if (flippedDiagonalDelta + settings.diagonalHysteresis < normalDiagonalDelta)
		{
			shouldFlip = true;
		}
		else if (normalDiagonalDelta + settings.diagonalHysteresis < flippedDiagonalDelta)
		{
			shouldFlip = false;
		}

		if (shouldFlip == currentlyFlipped)
		{
			return { CellAction::None, true };
		}

		return { shouldFlip ? CellAction::SetFlipped : CellAction::SetNormal, true };
	}

	CellBounds ComputeAffectedCellBounds(
		const Rect& dirtyVertices,
		const std::uint32_t maxCellX,
		const std::uint32_t maxCellZ) noexcept
	{
		if (dirtyVertices.left > dirtyVertices.right || dirtyVertices.top > dirtyVertices.bottom)
		{
			return {};
		}

		// A changed vertex affects the cell on either side of it. The dirty
		// rectangle is inclusive, so expanding its left/top edges covers the
		// preceding cells while its right/bottom edges already cover the other side.
		const std::int64_t left = std::max<std::int64_t>(0, static_cast<std::int64_t>(dirtyVertices.left) - 1);
		const std::int64_t top = std::max<std::int64_t>(0, static_cast<std::int64_t>(dirtyVertices.top) - 1);
		const std::int64_t right = std::min<std::int64_t>(dirtyVertices.right, maxCellX);
		const std::int64_t bottom = std::min<std::int64_t>(dirtyVertices.bottom, maxCellZ);

		if (left > right || top > bottom || right < 0 || bottom < 0)
		{
			return {};
		}

		return {
			static_cast<std::uint32_t>(left),
			static_cast<std::uint32_t>(top),
			static_cast<std::uint32_t>(right),
			static_cast<std::uint32_t>(bottom),
			true
		};
	}

	bool TryMapWorldToCell(
		const float worldX,
		const float worldZ,
		const float worldMaxX,
		const float worldMaxZ,
		const std::uint32_t maxCellX,
		const std::uint32_t maxCellZ,
		WorldCell& result) noexcept
	{
		if (!std::isfinite(worldX) || !std::isfinite(worldZ) ||
			!std::isfinite(worldMaxX) || !std::isfinite(worldMaxZ) ||
			worldX < 0.0F || worldZ < 0.0F ||
			worldX >= worldMaxX || worldZ >= worldMaxZ)
		{
			return false;
		}

		const float oneOverCellSize = 1.0F / kCellSize;
		const float cellXFloat = std::floor(worldX * oneOverCellSize);
		const float cellZFloat = std::floor(worldZ * oneOverCellSize);
		if (cellXFloat < 0.0F || cellZFloat < 0.0F ||
			cellXFloat > static_cast<float>(std::numeric_limits<std::uint32_t>::max()) ||
			cellZFloat > static_cast<float>(std::numeric_limits<std::uint32_t>::max()))
		{
			return false;
		}

		result.x = std::min(static_cast<std::uint32_t>(cellXFloat), maxCellX);
		result.z = std::min(static_cast<std::uint32_t>(cellZFloat), maxCellZ);
		result.localX = std::clamp((worldX - static_cast<float>(result.x) * kCellSize) * oneOverCellSize, 0.0F, 1.0F);
		result.localZ = std::clamp((worldZ - static_cast<float>(result.z) * kCellSize) * oneOverCellSize, 0.0F, 1.0F);
		return true;
	}

	float InterpolateFlippedCell(
		const CellHeights& heights,
		const float localX,
		const float localZ) noexcept
	{
		if (localX + localZ <= 1.0F)
		{
			return heights.h00 +
				(heights.h10 - heights.h00) * localX +
				(heights.h01 - heights.h00) * localZ;
		}

		return heights.h10 * (1.0F - localZ) +
			heights.h01 * (1.0F - localX) +
			heights.h11 * (localX + localZ - 1.0F);
	}

	std::optional<std::uintptr_t> DecodeRelativeCallTarget(
		const std::uintptr_t instructionAddress,
		const RelativeCallBytes& bytes) noexcept
	{
		if (bytes[0] != 0xE8)
		{
			return std::nullopt;
		}

		const std::uint32_t encoded =
			static_cast<std::uint32_t>(bytes[1]) |
			(static_cast<std::uint32_t>(bytes[2]) << 8) |
			(static_cast<std::uint32_t>(bytes[3]) << 16) |
			(static_cast<std::uint32_t>(bytes[4]) << 24);
		const std::int32_t displacement = std::bit_cast<std::int32_t>(encoded);
		return instructionAddress + bytes.size() + displacement;
	}

	std::optional<RelativeCallBytes> EncodeRelativeCall(
		const std::uintptr_t instructionAddress,
		const std::uintptr_t targetAddress) noexcept
	{
		const std::int64_t displacement =
			static_cast<std::int64_t>(targetAddress) -
			static_cast<std::int64_t>(instructionAddress) -
			static_cast<std::int64_t>(RelativeCallBytes{}.size());
		if (displacement < std::numeric_limits<std::int32_t>::min() ||
			displacement > std::numeric_limits<std::int32_t>::max())
		{
			return std::nullopt;
		}

		const std::uint32_t encoded = std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
		return RelativeCallBytes{
			0xE8,
			static_cast<std::uint8_t>(encoded),
			static_cast<std::uint8_t>(encoded >> 8),
			static_cast<std::uint8_t>(encoded >> 16),
			static_cast<std::uint8_t>(encoded >> 24)
		};
	}
}
