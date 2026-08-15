#include "SC4DjemFixDirector.hpp"

#include <cIGZFrameWork.h>

#include "utils/Logger.h"
#include "utils/Settings.h"
#include "SC4VersionDetection.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <wil/win32_helpers.h>

namespace {
    constexpr uint32_t kDirectorId = 0x320238B2u;
    static_assert(kDirectorId != 0u, "Run tools/rename_project.py or assign a unique director ID");
}

SC4DjemFixDirector::SC4DjemFixDirector() = default;
SC4DjemFixDirector::~SC4DjemFixDirector() = default;
uint32_t SC4DjemFixDirector::GetDirectorID() const { return kDirectorId; }

bool SC4DjemFixDirector::OnStart(cIGZCOM* pCOM)
{
    cRZMessage2COMDirector::OnStart(pCOM);
    if (auto* framework = RZGetFrameWork()) framework->AddHook(this);
    return true;
}

bool SC4DjemFixDirector::PreFrameWorkInit() { return true; }
bool SC4DjemFixDirector::PreAppInit() { return true; }

bool SC4DjemFixDirector::PostAppInit()
{
    InitializeLogger_();
    LOG_INFO("PostAppInit");
    LOG_INFO("Detected game version: {}", SC4VersionDetection::GetGameVersion());
    return true;
}

bool SC4DjemFixDirector::PreAppShutdown() { return true; }

bool SC4DjemFixDirector::PostAppShutdown()
{
    if (auto* framework = RZGetFrameWork()) framework->RemoveHook(this);
    Logger::Shutdown();
    return true;
}

bool SC4DjemFixDirector::PostSystemServiceShutdown() { return true; }
bool SC4DjemFixDirector::AbortiveQuit() { return true; }
bool SC4DjemFixDirector::OnInstall() { return true; }
bool SC4DjemFixDirector::DoMessage(cIGZMessage2*) { return true; }

std::filesystem::path SC4DjemFixDirector::GetUserPluginsPath_()
{
    try {
        const auto modulePath = wil::GetModuleFileNameW(wil::GetModuleInstanceHandle());
        return std::filesystem::path(modulePath.get()).parent_path();
    } catch (const wil::ResultException&) {
        return {};
    }
}

void SC4DjemFixDirector::InitializeLogger_()
{
    const auto pluginsPath = GetUserPluginsPath_();
    const auto settingsPath = pluginsPath / "SC4DjemFix.ini";
    Logger::Initialize("SC4DjemFix", pluginsPath.parent_path().string(), false);
    Settings settings;
    settings.Load(settingsPath);
    Logger::Shutdown();
    Logger::Initialize("SC4DjemFix", pluginsPath.parent_path().string(), settings.GetLogToFile());
    Logger::SetLevel(settings.GetLogLevel());
    LOG_INFO("Using settings file: {}", settingsPath.string());
}
