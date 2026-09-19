#pragma once

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <spdlog/spdlog.h>

class Logger {
  public:
    static std::shared_ptr<spdlog::logger> Get();
    static void Initialize(const std::string& logName = "SC4DjemFix", const std::filesystem::path& logFilePath = {},
                           bool logToFile = true);
    static void SetLevel(spdlog::level::level_enum logLevel);
    static void Shutdown();

    [[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);

  private:
    static std::unique_ptr<std::ofstream> s_logFile;
    static std::shared_ptr<spdlog::logger> s_logger;
    static std::string s_logName;
    static bool s_initialized;
};

#define LOG_TRACE(...) Logger::Get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) Logger::Get()->debug(__VA_ARGS__)
#define LOG_INFO(...) Logger::Get()->info(__VA_ARGS__)
#define LOG_WARN(...) Logger::Get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) Logger::Get()->error(__VA_ARGS__)
#define LOG_CRITICAL(...) Logger::Get()->critical(__VA_ARGS__)
