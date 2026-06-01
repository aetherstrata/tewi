#pragma once

#ifdef TEWI_LOG
#ifdef TEWI_TRACE
#define LOG_TRACE(...)    ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Trace, __VA_ARGS__)
#endif
#define LOG_DBG(...)      ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Debug, __VA_ARGS__)
#define LOG_INFO(...)     ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Info, __VA_ARGS__)
#define LOG_WARN(...)     ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Warning, __VA_ARGS__)
#define LOG_ERR(...)      ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Error, __VA_ARGS__)
#define LOG_CRITICAL(...) ::tewi::format_log(::std::source_location::current(), ::tewi::LogLevel::Critical, __VA_ARGS__)
#else
#define LOG_TRACE(...)    ((void)0)
#define LOG_DBG(...)      ((void)0)
#define LOG_INFO(...)     ((void)0)
#define LOG_WARN(...)     ((void)0)
#define LOG_ERR(...)      ((void)0)
#define LOG_CRITICAL(...) ((void)0)
#endif
