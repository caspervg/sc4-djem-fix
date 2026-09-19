/*
 * DJEM behavior adapted from sc4-render-services, branch re/djem,
 * commit c146049fd5ccab2c30d4156a731a06f2a8160285.
 * See THIRD_PARTY_NOTICES.txt and docs/reverse-engineering.md.
 */

#include "DjemFix.h"

#include "DjemTerrain.h"
#include "Patcher.h"
#include "utils/Logger.h"

#include <SC4Rect.h>
#include <cISTETerrainMap.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>

#if !defined(_M_IX86)
#error SC4DjemFix hook code requires the 32-bit MSVC x86 ABI.
#endif

static_assert(sizeof(void*) == 4, "SC4DjemFix only supports 32-bit SimCity 4");
static_assert(sizeof(std::uintptr_t) == 4, "SC4DjemFix absolute addresses require 32-bit pointers");

namespace {
constexpr std::uintptr_t kCalculateNormalsCallSite = 0x007498CD;
constexpr std::uintptr_t kCalculateNormalsOriginal = 0x00743B60;
constexpr std::uintptr_t kGetAltitudeVtableEntry = 0x00AB3D4C;
constexpr std::uintptr_t kGetAltitudeOriginal = 0x00741260;

constexpr Djem::RelativeCallBytes kExpectedCalculateNormalsCall{0xE8, 0x8E, 0xA2, 0xFF, 0xFF};
} // namespace

// The game calls static hook addresses and supplies no owner context. This is
// the only process-global link to the director-owned Fix instance.
Djem::Fix* Djem::Fix::activeInstance_ = nullptr;

void __fastcall Djem::Fix::CalculateNormalsHook(cISTETerrainMap* terrain, void*, SC4Rect<std::int32_t>* dirtyVertices) {
    Fix& fix = *activeInstance_;
    fix.originalCalculateNormals_(terrain, dirtyVertices);
    fix.terrainProcessor_.Apply(terrain, dirtyVertices);
}

float __fastcall Djem::Fix::GetAltitudeHook(cISTETerrainMap* terrain, void*, const float worldX, const float worldZ) {
    Fix& fix = *activeInstance_;
    const auto altitude = fix.terrainProcessor_.TryGetFlippedAltitude(terrain, worldX, worldZ);
    if (altitude) {
        return *altitude;
    }
    return fix.originalGetAltitude_(terrain, worldX, worldZ);
}

Djem::RelativeCallBytes Djem::Fix::ReadCallBytes() noexcept {
    RelativeCallBytes bytes{};
    std::memcpy(bytes.data(), reinterpret_cast<const void*>(kCalculateNormalsCallSite), bytes.size());
    return bytes;
}

bool Djem::Fix::GuardOriginalCallSite() noexcept {
    const RelativeCallBytes actual = ReadCallBytes();
    const auto target = DecodeRelativeCallTarget(kCalculateNormalsCallSite, actual);
    if (actual != kExpectedCalculateNormalsCall || target != kCalculateNormalsOriginal) {
        LOG_ERROR(
            "DJEM: Call guard failed at 0x{:08X}. Expected E8 8E A2 FF FF with target 0x{:08X}. No patch was written.",
            static_cast<std::uint32_t>(kCalculateNormalsCallSite),
            static_cast<std::uint32_t>(kCalculateNormalsOriginal));
        return false;
    }
    return true;
}

bool Djem::Fix::GuardVtableEntry(const std::uintptr_t expectedTarget, const char* operation) noexcept {
    const std::uintptr_t actualTarget = *reinterpret_cast<const std::uintptr_t*>(kGetAltitudeVtableEntry);
    if (actualTarget != expectedTarget) {
        LOG_ERROR("DJEM: The {} guard failed at vtable entry 0x{:08X}. Found 0x{:08X}, expected 0x{:08X}. No patch was "
                  "written.",
                  operation, static_cast<std::uint32_t>(kGetAltitudeVtableEntry),
                  static_cast<std::uint32_t>(actualTarget), static_cast<std::uint32_t>(expectedTarget));
        return false;
    }
    return true;
}

bool Djem::Fix::Install(const Settings& settings, const std::uint16_t gameVersion) {
    if (callHookInstalled_ || heightHookInstalled_) {
        return true;
    }
    if (gameVersion != kSupportedGameVersion) {
        LOG_WARN("DJEM: SimCity 4 version {} is not supported. Version {} is required. No game memory was changed.",
                 gameVersion, kSupportedGameVersion);
        return false;
    }

    if (!settings.enabled) {
        LOG_INFO("DJEM: The fix is disabled. No game memory was changed.");
        return true;
    }

    // Complete the read-only preflight before making either write. This makes a
    // mismatched executable or an incompatible patch fail closed.
    if (!GuardOriginalCallSite()) {
        return false;
    }
    if (settings.matchHeightQueriesToFlippedCells && !GuardVtableEntry(kGetAltitudeOriginal, "install")) {
        return false;
    }

    terrainProcessor_.Configure(settings);
    originalCalculateNormals_ = reinterpret_cast<CalculateNormalsFunction>(kCalculateNormalsOriginal);
    originalGetAltitude_ = reinterpret_cast<GetAltitudeFunction>(kGetAltitudeOriginal);
    activeInstance_ = this;
    terrainProcessor_.SetActive(true);

    try {
        Patcher::InstallCallHook(kCalculateNormalsCallSite, reinterpret_cast<void*>(&CalculateNormalsHook));
        callHookInstalled_ = true;

        if (settings.matchHeightQueriesToFlippedCells) {
            Patcher::InstallJumpTableHook(kGetAltitudeVtableEntry, reinterpret_cast<void*>(&GetAltitudeHook));
            heightHookInstalled_ = true;
        }
    } catch (...) {
        // A call-only installation is behaviorally safe, but installation is
        // transactional by policy: make a guarded attempt to return to the exact
        // original state before propagating the failure to the director boundary.
        const bool heightHookRestored = RestoreHeightHook();
        const bool callHookRestored = RestoreCallHook();
        (void)heightHookRestored;
        (void)callHookRestored;
        if (!callHookInstalled_ && !heightHookInstalled_) {
            terrainProcessor_.SetActive(false);
            activeInstance_ = nullptr;
        }
        throw;
    }

    LOG_INFO("DJEM: Installed the call hook at 0x{:08X}{}.", static_cast<std::uint32_t>(kCalculateNormalsCallSite),
             heightHookInstalled_ ? " and GetAltitude vtable hook" : "");
    return true;
}

void Djem::Fix::Shutdown() noexcept {
    const bool heightHookRestored = RestoreHeightHook();
    const bool callHookRestored = RestoreCallHook();
    (void)heightHookRestored;
    (void)callHookRestored;
    if (!callHookInstalled_ && !heightHookInstalled_) {
        terrainProcessor_.SetActive(false);
        activeInstance_ = nullptr;
    }
}

bool Djem::Fix::RestoreHeightHook() noexcept {
    if (!heightHookInstalled_) {
        return true;
    }

    const auto hookAddress = reinterpret_cast<std::uintptr_t>(&GetAltitudeHook);
    if (!GuardVtableEntry(hookAddress, "restore")) {
        return false;
    }

    try {
        Patcher::InstallJumpTableHook(kGetAltitudeVtableEntry, reinterpret_cast<void*>(kGetAltitudeOriginal));
        heightHookInstalled_ = false;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("DJEM: Failed to restore the GetAltitude vtable entry. {}", e.what());
    } catch (...) {
        LOG_ERROR("DJEM: Failed to restore the GetAltitude vtable entry because of an unknown error.");
    }
    return false;
}

bool Djem::Fix::RestoreCallHook() noexcept {
    if (!callHookInstalled_) {
        return true;
    }

    const auto expectedHookBytes =
        EncodeRelativeCall(kCalculateNormalsCallSite, reinterpret_cast<std::uintptr_t>(&CalculateNormalsHook));
    if (!expectedHookBytes || ReadCallBytes() != *expectedHookBytes) {
        LOG_ERROR("DJEM: The restore guard failed at call site 0x{:08X}. Another writer changed the hook. No patch was "
                  "written.",
                  static_cast<std::uint32_t>(kCalculateNormalsCallSite));
        return false;
    }

    try {
        Patcher::InstallCallHook(kCalculateNormalsCallSite, reinterpret_cast<void*>(kCalculateNormalsOriginal));
        callHookInstalled_ = false;
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("DJEM: Failed to restore the terrain call site. {}", e.what());
    } catch (...) {
        LOG_ERROR("DJEM: Failed to restore the terrain call site because of an unknown error.");
    }
    return false;
}
