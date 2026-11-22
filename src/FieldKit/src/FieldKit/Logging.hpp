#pragma once


#include <spdlog/spdlog.h>

#include <FieldKit/BuildMode.hpp>


/*
 * Logging uses spdlog's default logger and runtime level filter (info by default).
 * Calls preserve source locations and accept fmt-style format strings.
 * Trace and debug calls are compiled out in FK release mode, including arguments.
 */

#if FK_MODE_DEBUG
#define FK_LOG_TRACE(...)                                                                          \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::trace, __VA_ARGS__)
#define FK_LOG_DEBUG(...)                                                                          \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::debug, __VA_ARGS__)
#else
#define FK_LOG_TRACE(...) ((void)0)
#define FK_LOG_DEBUG(...) ((void)0)
#endif

#define FK_LOG_INFO(...)                                                                           \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::info, __VA_ARGS__)
#define FK_LOG_WARN(...)                                                                           \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::warn, __VA_ARGS__)
#define FK_LOG_ERROR(...)                                                                          \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::err, __VA_ARGS__)
#define FK_LOG_CRITICAL(...)                                                                       \
    SPDLOG_LOGGER_CALL(::spdlog::default_logger_raw(), ::spdlog::level::critical, __VA_ARGS__)
