#pragma once

#include "DjemTerrain.h"

#include <cstdint>

class cISTETerrainMap;
template <typename T> class SC4Rect;

namespace Djem
{
	class Fix final
	{
	public:
		Fix() = default;
		~Fix() = default;

		Fix(const Fix&) = delete;
		Fix& operator=(const Fix&) = delete;

		[[nodiscard]] bool Install(const Settings& settings, std::uint16_t gameVersion);
		void Shutdown() noexcept;

private:
		using CalculateNormalsFunction = void(__thiscall*)(cISTETerrainMap*, SC4Rect<std::int32_t>*);
		using GetAltitudeFunction = float(__thiscall*)(cISTETerrainMap*, float, float);

		static void __fastcall CalculateNormalsHook(
			cISTETerrainMap* terrain,
			void* edx,
			SC4Rect<std::int32_t>* dirtyVertices);
		static float __fastcall GetAltitudeHook(
			cISTETerrainMap* terrain,
			void* edx,
			float worldX,
			float worldZ);

		[[nodiscard]] static RelativeCallBytes ReadCallBytes() noexcept;
		[[nodiscard]] static bool GuardOriginalCallSite() noexcept;
		[[nodiscard]] static bool GuardVtableEntry(std::uintptr_t expectedTarget, const char* operation) noexcept;

		[[nodiscard]] bool RestoreHeightHook() noexcept;
		[[nodiscard]] bool RestoreCallHook() noexcept;

		static Fix* activeInstance_;
		static constexpr std::uint16_t kSupportedGameVersion = 641;

		TerrainProcessor terrainProcessor_{};
		CalculateNormalsFunction originalCalculateNormals_ = nullptr;
		GetAltitudeFunction originalGetAltitude_ = nullptr;
		bool callHookInstalled_ = false;
		bool heightHookInstalled_ = false;
	};
}
