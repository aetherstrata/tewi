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

void resetLogger() noexcept
{
    registerLogger(nullptr, nullptr);
}

void log(std::source_location loc, LogLevel lvl, std::string&& message) noexcept
{
    if (auto& [callback, user_data] = global_logger_state(); callback != nullptr)
    {
        callback(loc, lvl, std::move(message), user_data);
    }
}
} // namespace tewi
