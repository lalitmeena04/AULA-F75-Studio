#include "Logging.h"

#include <chrono>
#include <memory>
#include <vector>

#include <QDir>
#include <QStandardPaths>
#include <QtGlobal>

#include <spdlog/sinks/dup_filter_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#ifdef _WIN32
#include <spdlog/sinks/msvc_sink.h>
#endif

namespace F75 {

namespace {

void qtMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    spdlog::level::level_enum level = spdlog::level::debug;
    switch(type) {
    case QtDebugMsg:
        level = spdlog::level::debug;
        break;
    case QtInfoMsg:
        level = spdlog::level::info;
        break;
    case QtWarningMsg:
        level = spdlog::level::warn;
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = spdlog::level::err;
        break;
    }
    const spdlog::source_loc loc{context.file ? context.file : "qt", context.line,
                                 context.function ? context.function : ""};
    spdlog::default_logger_raw()->log(loc, level, "[qt] {}", message.toStdString());
}

} // namespace

QString logDir() {
#if SPDLOG_ACTIVE_LEVEL >= SPDLOG_LEVEL_OFF
    return {};
#else
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/logs");
#endif
}

void flushLogging() {
    spdlog::default_logger_raw()->flush();
}

QString initLogging() {
#if SPDLOG_ACTIVE_LEVEL >= SPDLOG_LEVEL_OFF
    return {};
#else
    const QString dir = logDir();
    QDir().mkpath(dir);
    const QString file = dir + QStringLiteral("/aulaf75studio.log");
    const auto level = static_cast<spdlog::level::level_enum>(SPDLOG_ACTIVE_LEVEL);

    try {
        std::vector<spdlog::sink_ptr> sinks;
        auto fileSink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            file.toStdString(), static_cast<std::size_t>(kLogFileMaxBytes), kLogFilesKept);
        auto dedup = std::make_shared<spdlog::sinks::dup_filter_sink_mt>(std::chrono::seconds(5));
        dedup->add_sink(fileSink);
        sinks.push_back(dedup);
#ifdef _WIN32
        sinks.push_back(std::make_shared<spdlog::sinks::msvc_sink_mt>());
#endif
        auto logger = std::make_shared<spdlog::logger>("aulaf75studio", sinks.begin(), sinks.end());
        logger->set_level(level);
        logger->flush_on(spdlog::level::warn);
        logger->set_pattern("%Y-%m-%d %H:%M:%S.%e [%L] [t:%t] %v  (%s:%#)");
        spdlog::set_default_logger(logger);
        spdlog::set_level(level);
        spdlog::flush_every(std::chrono::seconds(2));
    } catch(const std::exception&) {
        spdlog::set_level(level);
    }
    qInstallMessageHandler(qtMessageHandler);
    return file;
#endif
}

} // namespace F75
