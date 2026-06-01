export module tewi:logging;

import std;

export namespace tewi
{
enum class LogLevel { Trace, Debug, Info, Warning, Error, Critical };

using LoggerCallback = void (*)(std::source_location loc, LogLevel level, std::string_view msg,
                                void* userData);

void registerLogger(LoggerCallback callback, void* userData = nullptr) noexcept;
void resetLogger() noexcept;

void log(LogLevel lvl, std::string_view message,
         std::source_location loc = std::source_location::current()) noexcept;

void trace(std::string_view msg,
           std::source_location loc = std::source_location::current()) noexcept;

void debug(std::string_view msg,
           std::source_location loc = std::source_location::current()) noexcept;

void info(std::string_view msg,
          std::source_location loc = std::source_location::current()) noexcept;

void warn(std::string_view msg,
          std::source_location loc = std::source_location::current()) noexcept;

void error(std::string_view msg,
           std::source_location loc = std::source_location::current()) noexcept;

void critical(std::string_view msg,
              std::source_location loc = std::source_location::current()) noexcept;
} // namespace tewi
