module tewi;

import std;

struct logger_state
{
    tewi::LoggerCallback cb = nullptr;
    void* user_data         = nullptr;
};

static logger_state& global_logger_state()
{
    static logger_state s;
    return s;
}

namespace tewi
{
void registerLogger(LoggerCallback cb, void* user_data) noexcept
{
    auto& s = global_logger_state();

    s.cb        = cb;
    s.user_data = user_data;
}

void unregisterLogger() noexcept
{
    registerLogger(nullptr, nullptr);
}

void log(LogLevel lvl, std::string_view message, std::source_location loc) noexcept
{
    auto& s = global_logger_state();
    if (s.cb)
    {
        s.cb(loc, lvl, message, s.user_data);
    }
}

void trace(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Trace, msg, loc);
}
void debug(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Debug, msg, loc);
}
void info(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Info, msg, loc);
}
void warn(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Warning, msg, loc);
}
void error(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Error, msg, loc);
}
void critical(std::string_view msg, std::source_location loc) noexcept
{
    log(LogLevel::Critical, msg, loc);
}
} // namespace tewi
