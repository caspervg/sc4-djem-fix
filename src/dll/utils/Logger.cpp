#include "Logger.h"

#include <exception>
#include <vector>

#include "spdlog/sinks/msvc_sink.h"
#include "spdlog/sinks/ostream_sink.h"

namespace {
// spdlog is linked without SPDLOG_WCHAR_FILENAMES, so its file sinks open paths through
// the narrow CRT and mangle non-ASCII directories. Owning the stream keeps the path wide.
std::unique_ptr<std::ofstream> OpenLogFile(const std::filesystem::path& path) {
    if (path.empty()) {
        return nullptr;
    }

    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    auto stream = std::make_unique<std::ofstream>(path, std::ios::binary | std::ios::trunc);
    if (!stream->is_open()) {
        return nullptr;
    }
    return stream;
}
} // namespace

std::shared_ptr<spdlog::logger> Logger::Get() {
    if (!s_initialized) {
        Initialize();
    }
    return s_logger;
}

void Logger::Initialize(const std::string& logName, const std::filesystem::path& logFilePath, const bool logToFile) {
    if (s_initialized && s_logger) {
        return;
    }

    s_logName = logName;

    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());

    if (logToFile) {
        try {
            s_logFile = OpenLogFile(logFilePath);
        } catch (const std::exception&) {
            s_logFile.reset();
        }

        if (s_logFile) {
            sinks.push_back(std::make_shared<spdlog::sinks::ostream_sink_mt>(*s_logFile, false));
        }
    }

    s_logger = std::make_shared<spdlog::logger>(s_logName, sinks.begin(), sinks.end());
    spdlog::set_default_logger(s_logger);
    s_logger->set_level(spdlog::level::info);
    s_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
    s_logger->flush_on(spdlog::level::info);
    s_initialized = true;
}

void Logger::SetLevel(const spdlog::level::level_enum logLevel) {
    if (s_logger) {
        s_logger->set_level(logLevel);
        s_logger->flush_on(logLevel);
    }
}

void Logger::Shutdown() {
    if (s_logger) {
        s_logger->flush();
        s_logger.reset();
    }
    // Drops the default logger, destroying the sink that references s_logFile.
    spdlog::shutdown();
    s_logFile.reset();
    s_initialized = false;
}

std::string Logger::PathToUtf8(const std::filesystem::path& path) {
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

std::unique_ptr<std::ofstream> Logger::s_logFile = nullptr;
std::shared_ptr<spdlog::logger> Logger::s_logger = nullptr;
std::string Logger::s_logName = "SC4DjemFix";
bool Logger::s_initialized = false;
