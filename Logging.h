#pragma once

// Build sets SPDLOG_ACTIVE_LEVEL. Debug builds compile TRC_DEBUG in. Release builds start at INFO.
#include <spdlog/spdlog.h>

#include <QString>

#define TRC_DEBUG(...) SPDLOG_DEBUG(__VA_ARGS__)
#define TRC_INFO(...) SPDLOG_INFO(__VA_ARGS__)
#define TRC_WARN(...) SPDLOG_WARN(__VA_ARGS__)
#define TRC_ERROR(...) SPDLOG_ERROR(__VA_ARGS__)

namespace F75 {
QString initLogging();
QString logDir();
void flushLogging();
constexpr qint64 kLogFileMaxBytes = 2 * 1024 * 1024;
constexpr int kLogFilesKept = 3;
} // namespace F75
