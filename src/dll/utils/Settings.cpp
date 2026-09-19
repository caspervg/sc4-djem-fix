#include "Settings.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cmath>
#include <exception>
#include <optional>
#include <string>

#include "Logger.h"
#include "mini/ini.h"

namespace {
constexpr auto kDefaultLogLevel = spdlog::level::info;
constexpr bool kDefaultLogToFile = true;
constexpr auto kSectionName = "SC4DjemFix";

[[nodiscard]] std::string ToLower(std::string value) {
    std::ranges::transform(value, value.begin(),
                           [](const unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

[[nodiscard]] std::optional<spdlog::level::level_enum> ParseLogLevel(const std::string& value) {
    const std::string normalized = ToLower(value);
    if (normalized == "trace")
        return spdlog::level::trace;
    if (normalized == "debug")
        return spdlog::level::debug;
    if (normalized == "info")
        return spdlog::level::info;
    if (normalized == "warn" || normalized == "warning")
        return spdlog::level::warn;
    if (normalized == "error")
        return spdlog::level::err;
    if (normalized == "critical")
        return spdlog::level::critical;
    if (normalized == "off")
        return spdlog::level::off;
    return std::nullopt;
}

[[nodiscard]] std::optional<bool> ParseBool(const std::string& value) {
    const std::string normalized = ToLower(value);
    if (normalized == "true" || normalized == "1" || normalized == "yes")
        return true;
    if (normalized == "false" || normalized == "0" || normalized == "no")
        return false;
    return std::nullopt;
}

[[nodiscard]] std::optional<float> ParseNonNegativeFloat(const std::string& value) {
    float result = 0.0F;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size() || !std::isfinite(result) || result < 0.0F) {
        return std::nullopt;
    }
    return result;
}

[[nodiscard]] std::optional<std::uint32_t> ParseUInt32(const std::string& value) {
    std::uint32_t result = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        return std::nullopt;
    return result;
}

void LogInvalidValue(const char* name, const std::string& value, const std::filesystem::path& path) {
    LOG_WARN("Settings: {} has invalid value '{}'. The default value is used. File: {}", name, value, path.string());
}
} // namespace

Settings::Settings() : logLevel_(kDefaultLogLevel), logToFile_(kDefaultLogToFile), djemSettings_() {}

void Settings::Load(const std::filesystem::path& settingsFilePath) {
    *this = Settings();

    try {
        const mINI::INIFile file(settingsFilePath.string());
        mINI::INIStructure ini;

        if (!file.read(ini) || !ini.has(kSectionName)) {
            return;
        }

        const auto section = ini.get(kSectionName);

        const auto loadBool = [&](const char* name, bool& destination) {
            if (!section.has(name))
                return;
            const std::string value = section.get(name);
            if (const auto parsed = ParseBool(value))
                destination = *parsed;
            else
                LogInvalidValue(name, value, settingsFilePath);
        };

        if (section.has("LogLevel")) {
            const std::string value = section.get("LogLevel");
            if (const auto parsed = ParseLogLevel(value))
                logLevel_ = *parsed;
            else
                LogInvalidValue("LogLevel", value, settingsFilePath);
        }

        loadBool("LogToFile", logToFile_);
        loadBool("Enabled", djemSettings_.enabled);
        loadBool("ClearNonCandidates", djemSettings_.clearNonCandidates);
        loadBool("MatchHeightQueriesToFlippedCells", djemSettings_.matchHeightQueriesToFlippedCells);

        const auto loadFloat = [&](const char* name, float& destination) {
            if (!section.has(name))
                return;
            const std::string value = section.get(name);
            if (const auto parsed = ParseNonNegativeFloat(value))
                destination = *parsed;
            else
                LogInvalidValue(name, value, settingsFilePath);
        };
        loadFloat("MinHeightDelta", djemSettings_.minHeightDelta);
        loadFloat("DiagonalHysteresis", djemSettings_.diagonalHysteresis);

        if (section.has("LogEveryNChanges")) {
            const std::string value = section.get("LogEveryNChanges");
            if (const auto parsed = ParseUInt32(value))
                djemSettings_.logEveryNChanges = *parsed;
            else
                LogInvalidValue("LogEveryNChanges", value, settingsFilePath);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Settings: Could not read {}. {}", settingsFilePath.string(), e.what());
        *this = Settings();
    }
}

spdlog::level::level_enum Settings::GetLogLevel() const noexcept { return logLevel_; }

bool Settings::GetLogToFile() const noexcept { return logToFile_; }

const Djem::Settings& Settings::GetDjemSettings() const noexcept { return djemSettings_; }
