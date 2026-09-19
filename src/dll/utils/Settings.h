#pragma once

#include <filesystem>

#include <spdlog/common.h>

#include "DjemLogic.h"

class Settings {
  public:
    Settings();

    void Load(const std::filesystem::path& settingsFilePath);

    [[nodiscard]] spdlog::level::level_enum GetLogLevel() const noexcept;
    [[nodiscard]] bool GetLogToFile() const noexcept;
    [[nodiscard]] const Djem::Settings& GetDjemSettings() const noexcept;

  private:
    spdlog::level::level_enum logLevel_;
    bool logToFile_;
    Djem::Settings djemSettings_;
};
