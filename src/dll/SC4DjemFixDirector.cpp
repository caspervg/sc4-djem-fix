#include "SC4DjemFixDirector.hpp"

#include <cIGZFrameWork.h>

#include "SC4VersionDetection.h"
#include "utils/Logger.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <exception>
#include <wil/win32_helpers.h>

namespace {
constexpr auto kDirectorId = 0x320238B2u;
constexpr uint16_t kSupportedGameVersion = 641;
constexpr auto kLogFileName = "SC4DjemFix.log";
} // namespace

SC4DjemFixDirector::SC4DjemFixDirector() = default;
SC4DjemFixDirector::~SC4DjemFixDirector() = default;
uint32_t SC4DjemFixDirector::GetDirectorID() const { return kDirectorId; }

bool SC4DjemFixDirector::OnStart(cIGZCOM* pCOM) {
    cRZMessage2COMDirector::OnStart(pCOM);
    if (auto* framework = RZGetFrameWork()) {
        framework->AddHook(this);
        frameworkHookInstalled_ = true;
    }
    return true;
}

bool SC4DjemFixDirector::PreFrameWorkInit() { return true; }
bool SC4DjemFixDirector::PreAppInit() { return true; }

bool SC4DjemFixDirector::PostAppInit() {
    InitializeLogger_();
    const uint16_t gameVersion = SC4VersionDetection::GetGameVersion();
    LOG_INFO("SC4DjemFix: Detected SimCity 4 version {}.", gameVersion);
    if (gameVersion != kSupportedGameVersion) {
        LOG_WARN("SC4DjemFix: Version {} is not supported. This DLL supports version {} only. No patch was written.",
                 gameVersion, kSupportedGameVersion);
        return true;
    }

    const Djem::Settings& djemSettings = settings_.GetDjemSettings();
    LOG_INFO("SC4DjemFix: Settings loaded. Enabled={}, ClearNonCandidates={}, MatchHeightQueriesToFlippedCells={}, "
             "MinHeightDelta={}, DiagonalHysteresis={}, LogEveryNChanges={}.",
             djemSettings.enabled, djemSettings.clearNonCandidates, djemSettings.matchHeightQueriesToFlippedCells,
             djemSettings.minHeightDelta, djemSettings.diagonalHysteresis, djemSettings.logEveryNChanges);

    try {
        if (!djemFix_.Install(djemSettings, gameVersion))
            LOG_ERROR("SC4DjemFix: The patch guards failed. No patch was written.");
    } catch (const std::exception& e) {
        LOG_ERROR("SC4DjemFix: Could not install the fix. {}", e.what());
    } catch (...) {
        LOG_ERROR("SC4DjemFix: Could not install the fix because of an unknown error.");
    }
    return true;
}

bool SC4DjemFixDirector::PreAppShutdown() { return true; }

bool SC4DjemFixDirector::PostAppShutdown() {
    Shutdown_();
    return true;
}

bool SC4DjemFixDirector::PostSystemServiceShutdown() { return true; }
bool SC4DjemFixDirector::AbortiveQuit() {
    Shutdown_();
    return true;
}
bool SC4DjemFixDirector::OnInstall() { return true; }
bool SC4DjemFixDirector::DoMessage(cIGZMessage2*) { return true; }

std::filesystem::path SC4DjemFixDirector::GetDllDirectory_() {
    try {
        const auto modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());
        return std::filesystem::path(modulePath.get()).parent_path();
    } catch (const wil::ResultException&) {
        return {};
    }
}

void SC4DjemFixDirector::InitializeLogger_() {
    const auto dllDirectory = GetDllDirectory_();
    const auto settingsPath = dllDirectory / "SC4DjemFix.ini";

    // Init throws when cISC4App is unavailable or the directory cannot be created.
    std::filesystem::path logFilePath;
    std::string logDirectoryError;
    try {
        logDirectoryManager_.Init();
        logFilePath = logDirectoryManager_.GetLogFilePath(kLogFileName);
    } catch (const std::exception& e) {
        logDirectoryError = e.what();
    } catch (...) {
        logDirectoryError = "An unknown error occurred.";
    }
    if (!logDirectoryError.empty())
        logFilePath = dllDirectory / kLogFileName;

    Logger::Initialize("SC4DjemFix", logFilePath, false);
    settings_.Load(settingsPath);
    Logger::Shutdown();
    Logger::Initialize("SC4DjemFix", logFilePath, settings_.GetLogToFile());
    Logger::SetLevel(settings_.GetLogLevel());
    if (!logDirectoryError.empty()) {
        LOG_WARN("SC4DjemFix: Could not use the game log directory. {} Using {} instead.", logDirectoryError,
                 Logger::PathToUtf8(logFilePath));
    }
    LOG_INFO("SC4DjemFix: Using settings file {}.", Logger::PathToUtf8(settingsPath));
}

void SC4DjemFixDirector::Shutdown_() noexcept {
    djemFix_.Shutdown();
    if (frameworkHookInstalled_) {
        if (auto* framework = RZGetFrameWork())
            framework->RemoveHook(this);
        frameworkHookInstalled_ = false;
    }
    Logger::Shutdown();
}
