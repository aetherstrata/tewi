export module tewi:logging;

import std;

export namespace tewi
{
/// Severity levels for log messages, ordered from least to most severe.
enum class LogLevel {
    Trace,   ///< Fine-grained diagnostic events
    Debug,   ///< Debug information useful during development
    Info,    ///< General informational messages
    Warning, ///< Warnings about unexpected but recoverable situations
    Error,   ///< Errors indicating an operation failed
    Critical ///< Critical failures requiring immediate attention
};


/**
 * @brief Signature of a user-provided logging callback.
 *
 * Registered callbacks receive source location metadata, the log severity,
 * the formatted message text, and the user-defined pointer supplied during
 * registration.
 *
 * @param loc Source location associated with the log event.
 * @param level Severity level of the log message.
 * @param msg Formatted log message text.
 * @param userData Opaque user pointer provided at logger registration time.
 */
using LoggerCallback = void (*)(std::source_location loc, LogLevel level, std::string&& msg,
                                void* userData);

/**
 * @brief Registers a custom logger callback.
 *
 * Replaces the currently active logger with the specified callback and
 * associates an optional user-defined pointer that will be passed back
 * to the callback on each log event.
 *
 * @param callback Callback invoked for subsequent log messages.
 * @param userData Optional opaque pointer forwarded to the callback.
 *
 * @note Passing @c nullptr as @p userData is allowed.
 */
void registerLogger(LoggerCallback callback, void* userData = nullptr) noexcept;

/**
 * @brief Restores the default logging behavior.
 *
 * Clears any previously registered custom logger callback and associated
 * user data.
 */
void resetLogger() noexcept;

/**
 * @brief Emits a log message.
 *
 * Forwards the provided message, severity level, and source location to the
 * currently active logger implementation.
 *
 * @param loc Source location associated with the message.
 * @param level Severity level of the message.
 * @param message Message text to log.
 */
void log(std::source_location loc, LogLevel level, std::string&& message) noexcept;

/**
 * @brief Formats and emits a log message.
 *
 * Creates a formatted string from the given format string and arguments, then
 * forwards the resulting message to @ref log.
 *
 * @tparam Args Types of the formatting arguments.
 * @param loc Source location associated with the message.
 * @param lvl Severity level of the message.
 * @param fmt Format string used to build the final message.
 * @param args Arguments consumed by the format string.
 */
template <typename... Args>
void format_log(std::source_location loc, LogLevel lvl, std::format_string<Args...> fmt,
                Args&&... args) noexcept
{
    log(loc, lvl, std::format(fmt, std::forward<Args>(args)...));
}
} // namespace tewi
