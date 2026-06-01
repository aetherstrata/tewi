#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>

#include <utility>

import tewi;
import std;

class LoggerSetup : public Catch::EventListenerBase
{
public:
    using Catch::EventListenerBase::EventListenerBase;

    // called exactly once before all tests
    void testRunStarting(Catch::TestRunInfo const&) override
    {
        tewi::registerLogger([](std::source_location loc, tewi::LogLevel level, std::string&& msg, void* user_data)
        {
            static auto getLevelString = [](tewi::LogLevel lvl)
            {
                switch (lvl)
                {
                case tewi::LogLevel::Trace:    return "TRACE";
                case tewi::LogLevel::Debug:    return "DEBUG";
                case tewi::LogLevel::Info:     return "INFO";
                case tewi::LogLevel::Warning:  return "WARN";
                case tewi::LogLevel::Error:    return "ERROR";
                case tewi::LogLevel::Critical: return "CRITICAL";
                }
                std::unreachable();
            };

            // Forward Catch2's log messages to the console with a simple format.
            // This allows us to see log output in test runners that capture stdout.
            std::string_view level_str = getLevelString(level);
            std::cout << "[" << level_str << "] "
                      << loc.file_name() << ":" << loc.line() << " - "
                      << msg << std::endl;
        });
    }

    // called exactly once after all tests
    void testRunEnded(Catch::TestRunStats const&) override
    {
        tewi::resetLogger();
    }
};

CATCH_REGISTER_LISTENER(LoggerSetup)
