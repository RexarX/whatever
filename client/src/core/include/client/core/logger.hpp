#pragma once

#include <client/core/pch.hpp>

#include <client/core/core.hpp>

#include <ctti/type_id.hpp>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutex>
#include <QString>
#include <QTextStream>
#include <QtLogging>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace client {

/**
 * @brief Log severity levels.
 */
enum class LogLevel : uint8_t {
  kTrace = 0,
  kDebug = 1,
  kInfo = 2,
  kWarn = 3,
  kError = 4,
  kCritical = 5,
};

/**
 * @brief Converts LogLevel to a human-readable string.
 * @param level The log level to convert.
 * @return A string view representing the log level.
 */
[[nodiscard]] constexpr std::string_view LogLevelToString(LogLevel level) noexcept {
  switch (level) {
    case LogLevel::kTrace:
      return "TRACE";
    case LogLevel::kDebug:
      return "DEBUG";
    case LogLevel::kInfo:
      return "INFO";
    case LogLevel::kWarn:
      return "WARN";
    case LogLevel::kError:
      return "ERROR";
    case LogLevel::kCritical:
      return "CRITICAL";
    default:
      return "UNKNOWN";
  }
}

/**
 * @brief Configuration for logger behavior and output.
 */
struct LoggerConfig {
  std::string log_directory = "logs";                                    ///< Log output directory path.
  std::string file_name_pattern = "{name}_{timestamp}.log";              ///< Pattern for log file names.
  std::string console_pattern = "[{time}] [{level}] {name}: {message}";  ///< Console log pattern.
  std::string file_pattern = "[{time}] [{level}] {name}: {message}";     ///< File log pattern.

  size_t max_file_size = 0;                     ///< Maximum size of a single log file in bytes (0 = no limit).
  size_t max_files = 0;                         ///< Maximum number of log files to keep (0 = no limit).
  LogLevel auto_flush_level = LogLevel::kWarn;  ///< Minimum log level to flush automatically.

  bool enable_console = true;  ///< Enable console output.
  bool enable_file = true;     ///< Enable file output.
  bool truncate_files = true;  ///< Enable truncation of existing log files.
  bool async_logging = false;  ///< Enable async logging (not fully implemented with Qt).

  LogLevel source_location_level = LogLevel::kError;  ///< Minimum level to include source location.
  LogLevel stack_trace_level = LogLevel::kCritical;   ///< Minimum level to include stack trace.

  /**
   * @brief Creates default configuration.
   * @return Default LoggerConfig instance
   */
  [[nodiscard]] static LoggerConfig Default() noexcept { return {}; }

  /**
   * @brief Creates configuration for console-only output.
   * @return LoggerConfig instance for console-only logging
   */
  [[nodiscard]] static LoggerConfig ConsoleOnly() noexcept {
    LoggerConfig config;
    config.enable_console = true;
    config.enable_file = false;
    return config;
  }

  /**
   * @brief Creates configuration for file-only output.
   * @return LoggerConfig instance for file-only logging
   */
  [[nodiscard]] static LoggerConfig FileOnly() noexcept {
    LoggerConfig config;
    config.enable_console = false;
    config.enable_file = true;
    return config;
  }

  /**
   * @brief Creates configuration optimized for debug builds.
   * @return LoggerConfig instance for debug logging
   */
  [[nodiscard]] static LoggerConfig Debug() noexcept {
    LoggerConfig config;
    config.enable_console = true;
    config.enable_file = true;
    config.async_logging = false;
    return config;
  }

  /**
   * @brief Creates configuration optimized for release builds.
   * @return LoggerConfig instance for release logging
   */
  [[nodiscard]] static LoggerConfig Release() noexcept {
    LoggerConfig config;
    config.enable_console = false;
    config.enable_file = true;
    config.async_logging = true;
    return config;
  }
};

/**
 * @brief Type alias for logger type IDs.
 */
using LoggerId = size_t;

/**
 * @brief Trait to identify valid logger types.
 * @details A valid logger type must be an empty struct or class with a Name() function.
 * @tparam T Type to check
 */
template <typename T>
concept LoggerTrait = std::is_empty_v<std::remove_cvref_t<T>> && requires {
  { T::Name() } -> std::same_as<std::string_view>;
};

/**
 * @brief Trait to identify loggers with custom configuration.
 * @details A logger with config trait must satisfy LoggerTrait and provide Config().
 * @tparam T Type to check
 */
template <typename T>
concept LoggerWithConfigTrait = LoggerTrait<T> && requires {
  { T::Config() } -> std::same_as<LoggerConfig>;
};

/**
 * @brief Gets unique type ID for a logger type.
 * @tparam T Logger type
 * @return Unique type ID for the logger
 */
template <LoggerTrait T>
constexpr LoggerId LoggerIdOf() noexcept {
  return ctti::type_index_of<T>().hash();
}

/**
 * @brief Gets the name of a logger type.
 * @tparam T Logger type
 * @return Name of the logger
 */
template <LoggerTrait T>
constexpr std::string_view LoggerNameOf() noexcept {
  return T::Name();
}

/**
 * @brief Gets the configuration for a logger type.
 * @tparam T Logger type
 * @return Configuration for the logger
 */
template <LoggerTrait T>
inline LoggerConfig LoggerConfigOf() noexcept {
  if constexpr (LoggerWithConfigTrait<T>) {
    return T::Config();
  } else {
    return LoggerConfig::Default();
  }
}

/**
 * @brief Default logger type.
 */
struct DefaultLogger {
  static constexpr std::string_view Name() noexcept { return "APP"; }

  static LoggerConfig Config() noexcept {
#if defined(CLIENT_RELEASE_MODE)
    return LoggerConfig::Release();
#else
    return LoggerConfig::Debug();
#endif
  }
};

/**
 * @brief Constexpr instance of the default logger for easier user interface.
 */
inline constexpr DefaultLogger kDefaultLogger{};

namespace details {

/**
 * @brief Extracts the file name from a given path.
 * @param path The full file path.
 * @return The file name portion of the path.
 */
[[nodiscard]] constexpr std::string_view GetFileName(std::string_view path) noexcept {
  const size_t last_slash = path.find_last_of("/\\");
  return (last_slash != std::string_view::npos) ? path.substr(last_slash + 1) : path;
}

}  // namespace details

/**
 * @brief Centralized logging system with configurable output and formatting.
 * @details Uses Qt for file I/O and console output. Thread-safe via shared_mutex.
 * @note Thread-safe.
 */
class Logger {
public:
  Logger(const Logger&) = delete;
  Logger(Logger&&) = delete;
  ~Logger() noexcept = default;

  Logger& operator=(const Logger&) = delete;
  Logger& operator=(Logger&&) = delete;

  /**
   * @brief Adds a logger with the specified type and configuration.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @param config Configuration for the logger
   */
  template <LoggerTrait T>
  void AddLogger(T logger = {}, LoggerConfig config = LoggerConfigOf<T>()) noexcept;

  /**
   * @brief Removes a logger with the given type.
   * @note Cannot remove the default logger
   * @tparam T Logger type
   * @param logger Logger type instance
   */
  template <LoggerTrait T>
  void RemoveLogger(T logger = {}) noexcept;

  /**
   * @brief Flushes all registered loggers.
   */
  void FlushAll() noexcept;

  /**
   * @brief Flushes a specific logger.
   * @tparam T Logger type
   * @param logger Logger type instance
   */
  template <LoggerTrait T>
  void Flush(T logger = {}) noexcept;

  /**
   * @brief Logs a string message with typed logger.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @param level Log severity level
   * @param loc Source location where the log was triggered
   * @param message Message to log
   */
  template <LoggerTrait T>
  void LogMessage(T logger, LogLevel level, const std::source_location& loc, std::string_view message) noexcept;

  /**
   * @brief Logs a formatted message with typed logger.
   * @tparam T Logger type
   * @tparam Args Types of the format arguments
   * @param logger Logger type instance
   * @param level Log severity level
   * @param loc Source location where the log was triggered
   * @param fmt Format string
   * @param args Arguments for the format string
   */
  template <LoggerTrait T, typename... Args>
    requires(sizeof...(Args) > 0)
  void LogMessage(T logger, LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt,
                  Args&&... args) noexcept;

  /**
   * @brief Logs a string message with default logger.
   * @param level Log severity level
   * @param loc Source location where the log was triggered
   * @param message Message to log
   */
  void LogMessage(LogLevel level, const std::source_location& loc, std::string_view message) noexcept;

  /**
   * @brief Logs a formatted message with default logger.
   * @tparam Args Types of the format arguments
   * @param level Log severity level
   * @param loc Source location where the log was triggered
   * @param fmt Format string
   * @param args Arguments for the format string
   */
  template <typename... Args>
    requires(sizeof...(Args) > 0)
  void LogMessage(LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt,
                  Args&&... args) noexcept;

  /**
   * @brief Logs assertion failure with typed logger.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @param condition The failed condition as a string
   * @param loc Source location where the assertion failed
   * @param message Additional message describing the failure
   */
  template <LoggerTrait T>
  void LogAssertionFailure(T logger, std::string_view condition, const std::source_location& loc,
                           std::string_view message) noexcept;

  /**
   * @brief Logs assertion failure with typed logger (formatted).
   * @tparam T Logger type
   * @tparam Args Types of the format arguments
   * @param logger Logger type instance
   * @param condition The failed condition as a string
   * @param loc Source location where the assertion failed
   * @param fmt Format string for the failure message
   * @param args Arguments for the format string
   */
  template <LoggerTrait T, typename... Args>
    requires(sizeof...(Args) > 0)
  void LogAssertionFailure(T logger, std::string_view condition, const std::source_location& loc,
                           std::format_string<Args...> fmt, Args&&... args) noexcept;

  /**
   * @brief Logs assertion failure with default logger.
   * @param condition The failed condition as a string
   * @param loc Source location where the assertion failed
   * @param message Additional message describing the failure
   */
  void LogAssertionFailure(std::string_view condition, const std::source_location& loc,
                           std::string_view message) noexcept;

  /**
   * @brief Logs assertion failure with default logger (formatted).
   * @tparam Args Types of the format arguments
   * @param condition The failed condition as a string
   * @param loc Source location where the assertion failed
   * @param fmt Format string for the failure message
   * @param args Arguments for the format string
   */
  template <typename... Args>
    requires(sizeof...(Args) > 0)
  void LogAssertionFailure(std::string_view condition, const std::source_location& loc, std::format_string<Args...> fmt,
                           Args&&... args) noexcept;

  /**
   * @brief Sets the global default configuration for new loggers.
   * @param config The configuration to use as default
   */
  void SetDefaultConfig(const LoggerConfig& config) noexcept { default_config_ = config; }

  /**
   * @brief Sets the minimum log level for a typed logger.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @param level Minimum log level to set
   */
  template <LoggerTrait T>
  void SetLevel(T logger, LogLevel level) noexcept;

  /**
   * @brief Sets the minimum log level for the default logger.
   * @param level Minimum log level to set
   */
  void SetLevel(LogLevel level) noexcept;

  /**
   * @brief Checks if a logger with the given type exists.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @return True if the logger exists, false otherwise
   */
  template <LoggerTrait T>
  [[nodiscard]] bool HasLogger(T logger = {}) const noexcept;

  /**
   * @brief Checks if the default logger should log messages at the given level.
   * @param level The log level to check
   * @return True if messages at this level should be logged
   */
  [[nodiscard]] bool ShouldLog(LogLevel level) const noexcept;

  /**
   * @brief Checks if a typed logger should log messages at the given level.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @param level The log level to check
   * @return True if messages at this level should be logged
   */
  template <LoggerTrait T>
  [[nodiscard]] bool ShouldLog(T logger, LogLevel level) const noexcept;

  /**
   * @brief Gets the current log level for a typed logger.
   * @tparam T Logger type
   * @param logger Logger type instance
   * @return The current log level, or LogLevel::kTrace if logger doesn't exist
   */
  template <LoggerTrait T>
  [[nodiscard]] LogLevel GetLevel(T logger = {}) const noexcept;

  /**
   * @brief Gets the current log level for the default logger.
   * @return The current log level
   */
  [[nodiscard]] LogLevel GetLevel() const noexcept;

  /**
   * @brief Gets the current default configuration.
   * @return The default logger configuration
   */
  [[nodiscard]] const LoggerConfig& GetDefaultConfig() const noexcept { return default_config_; }

  /**
   * @brief Gets the singleton instance.
   * @return Reference to the Logger instance
   */
  [[nodiscard]] static Logger& GetInstance() noexcept {
    static Logger instance;
    return instance;
  }

private:
  /**
   * @brief Internal logger data for a single registered logger.
   */
  struct LoggerData {
    std::string name;
    LoggerConfig config;
    LogLevel level = LogLevel::kTrace;
    std::unique_ptr<QFile> file;
    std::unique_ptr<QTextStream> file_stream;
    QMutex file_mutex;

    LoggerData() = default;
    LoggerData(std::string n, LoggerConfig cfg) : name(std::move(n)), config(std::move(cfg)) {}
    LoggerData(const LoggerData&) = delete;
    LoggerData(LoggerData&&) = delete;
    ~LoggerData() = default;

    LoggerData& operator=(const LoggerData&) = delete;
    LoggerData& operator=(LoggerData&&) = delete;
  };

  Logger() noexcept;

  void FlushImpl(LoggerId logger_id) noexcept;
  void SetLevelImpl(LoggerId logger_id, LogLevel level) noexcept;
  [[nodiscard]] bool ShouldLogImpl(LoggerId logger_id, LogLevel level) const noexcept;
  [[nodiscard]] LogLevel GetLevelImpl(LoggerId logger_id) const noexcept;

  void LogMessageImpl(LoggerId logger_id, LogLevel level, const std::source_location& loc,
                      std::string_view message) noexcept;

  void LogAssertionFailureImpl(LoggerId logger_id, std::string_view condition, const std::source_location& loc,
                               std::string_view message) noexcept;

  [[nodiscard]] static std::string FormatLogFileName(std::string_view logger_name, std::string_view pattern) noexcept;
  [[nodiscard]] static std::string FormatLogMessage(const LoggerData& data, LogLevel level,
                                                    const std::source_location& loc, std::string_view message) noexcept;

  void WriteToConsole(LogLevel level, std::string_view message) noexcept;
  void WriteToFile(LoggerData& data, std::string_view message) noexcept;

  [[nodiscard]] static std::string CaptureStackTrace() noexcept;

  std::unordered_map<LoggerId, std::unique_ptr<LoggerData>> loggers_;
  mutable std::shared_mutex loggers_mutex_;
  LoggerConfig default_config_;
};

// ============================================================================
// Inline Implementation
// ============================================================================

inline Logger::Logger() noexcept {
  default_config_ = LoggerConfigOf<DefaultLogger>();

  constexpr LoggerId default_id = LoggerIdOf<DefaultLogger>();
  constexpr std::string_view default_name = LoggerNameOf<DefaultLogger>();

  auto data = std::make_unique<LoggerData>(std::string(default_name), default_config_);

  // Set up file output if enabled
  if (default_config_.enable_file) {
    QDir().mkpath(QString::fromStdString(default_config_.log_directory));
    std::string filename = FormatLogFileName(default_name, default_config_.file_name_pattern);
    auto filepath = QString::fromStdString(default_config_.log_directory) + "/" + QString::fromStdString(filename);

    data->file = std::make_unique<QFile>(filepath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (!default_config_.truncate_files) {
      mode |= QIODevice::Append;
    }
    if (data->file->open(mode)) {
      data->file_stream = std::make_unique<QTextStream>(data->file.get());
    }
  }

  loggers_.emplace(default_id, std::move(data));
}

template <LoggerTrait T>
inline void Logger::AddLogger(T /*logger*/, LoggerConfig config) noexcept {
  constexpr LoggerId logger_id = LoggerIdOf<T>();
  constexpr std::string_view logger_name = LoggerNameOf<T>();

  const std::scoped_lock lock(loggers_mutex_);
  if (loggers_.contains(logger_id)) {
    return;
  }

  auto data = std::make_unique<LoggerData>(std::string(logger_name), config);

  // Set up file output if enabled
  if (config.enable_file) {
    QDir().mkpath(QString::fromStdString(config.log_directory));
    std::string filename = FormatLogFileName(logger_name, config.file_name_pattern);
    auto filepath = QString::fromStdString(config.log_directory) + "/" + QString::fromStdString(filename);

    data->file = std::make_unique<QFile>(filepath);
    QIODevice::OpenMode mode = QIODevice::WriteOnly | QIODevice::Text;
    if (!config.truncate_files) {
      mode |= QIODevice::Append;
    }
    if (data->file->open(mode)) {
      data->file_stream = std::make_unique<QTextStream>(data->file.get());
    }
  }

  loggers_.emplace(logger_id, std::move(data));
}

template <LoggerTrait T>
inline void Logger::RemoveLogger(T /*logger*/) noexcept {
  constexpr LoggerId logger_id = LoggerIdOf<T>();

  // Cannot remove the default logger
  if (logger_id == LoggerIdOf<DefaultLogger>()) {
    return;
  }

  const std::scoped_lock lock(loggers_mutex_);
  if (const auto it = loggers_.find(logger_id); it != loggers_.end()) {
    if (it->second && it->second->file_stream) {
      it->second->file_stream->flush();
    }
    loggers_.erase(it);
  }
}

inline void Logger::FlushAll() noexcept {
  const std::shared_lock lock(loggers_mutex_);
  for (auto& [_, data] : loggers_) {
    if (data && data->file_stream) {
      const QMutexLocker file_lock(&data->file_mutex);
      data->file_stream->flush();
    }
  }
}

template <LoggerTrait T>
inline void Logger::Flush(T /*logger*/) noexcept {
  FlushImpl(LoggerIdOf<T>());
}

inline void Logger::FlushImpl(LoggerId logger_id) noexcept {
  const std::shared_lock lock(loggers_mutex_);
  if (const auto it = loggers_.find(logger_id); it != loggers_.end() && it->second && it->second->file_stream) {
    const QMutexLocker file_lock(&it->second->file_mutex);
    it->second->file_stream->flush();
  }
}

template <LoggerTrait T>
inline void Logger::LogMessage(T /*logger*/, LogLevel level, const std::source_location& loc,
                               std::string_view message) noexcept {
  LogMessageImpl(LoggerIdOf<T>(), level, loc, message);
}

template <LoggerTrait T, typename... Args>
  requires(sizeof...(Args) > 0)
inline void Logger::LogMessage(T logger, LogLevel level, const std::source_location& loc,
                               std::format_string<Args...> fmt, Args&&... args) noexcept {
  try {
    const std::string message = std::format(fmt, std::forward<Args>(args)...);
    LogMessage(logger, level, loc, message);
  } catch (...) {
    // Silently ignore formatting errors
  }
}

inline void Logger::LogMessage(LogLevel level, const std::source_location& loc, std::string_view message) noexcept {
  LogMessageImpl(LoggerIdOf<DefaultLogger>(), level, loc, message);
}

template <typename... Args>
  requires(sizeof...(Args) > 0)
inline void Logger::LogMessage(LogLevel level, const std::source_location& loc, std::format_string<Args...> fmt,
                               Args&&... args) noexcept {
  try {
    const std::string message = std::format(fmt, std::forward<Args>(args)...);
    LogMessage(level, loc, message);
  } catch (...) {
    // Silently ignore formatting errors
  }
}

inline void Logger::LogMessageImpl(LoggerId logger_id, LogLevel level, const std::source_location& loc,
                                   std::string_view message) noexcept {
  const std::shared_lock lock(loggers_mutex_);
  const auto it = loggers_.find(logger_id);
  if (it == loggers_.end() || !it->second) {
    return;
  }

  auto& data = *it->second;
  if (level < data.level) {
    return;
  }

  try {
    const std::string formatted = FormatLogMessage(data, level, loc, message);

    if (data.config.enable_console) {
      WriteToConsole(level, formatted);
    }

    if (data.config.enable_file && data.file_stream) {
      WriteToFile(data, formatted);
      if (level >= data.config.auto_flush_level) {
        const QMutexLocker file_lock(&data.file_mutex);
        data.file_stream->flush();
      }
    }
  } catch (...) {
    // Silently ignore logging errors
  }
}

template <LoggerTrait T>
inline void Logger::LogAssertionFailure(T /*logger*/, std::string_view condition, const std::source_location& loc,
                                        std::string_view message) noexcept {
  LogAssertionFailureImpl(LoggerIdOf<T>(), condition, loc, message);
}

template <LoggerTrait T, typename... Args>
  requires(sizeof...(Args) > 0)
inline void Logger::LogAssertionFailure(T logger, std::string_view condition, const std::source_location& loc,
                                        std::format_string<Args...> fmt, Args&&... args) noexcept {
  try {
    const std::string message = std::format(fmt, std::forward<Args>(args)...);
    LogAssertionFailure(logger, condition, loc, message);
  } catch (...) {
    LogAssertionFailure(logger, condition, loc, "Formatting error in assertion message");
  }
}

inline void Logger::LogAssertionFailure(std::string_view condition, const std::source_location& loc,
                                        std::string_view message) noexcept {
  LogAssertionFailureImpl(LoggerIdOf<DefaultLogger>(), condition, loc, message);
}

template <typename... Args>
  requires(sizeof...(Args) > 0)
inline void Logger::LogAssertionFailure(std::string_view condition, const std::source_location& loc,
                                        std::format_string<Args...> fmt, Args&&... args) noexcept {
  try {
    const std::string message = std::format(fmt, std::forward<Args>(args)...);
    LogAssertionFailure(condition, loc, message);
  } catch (...) {
    LogAssertionFailure(condition, loc, "Formatting error in assertion message");
  }
}

inline void Logger::LogAssertionFailureImpl(LoggerId logger_id, std::string_view condition,
                                            const std::source_location& loc, std::string_view message) noexcept {
  try {
    std::string assertion_msg = std::format("Assertion failed: {} | {}", condition, message);
    LogMessageImpl(logger_id, LogLevel::kCritical, loc, assertion_msg);
  } catch (...) {
    // Silently ignore logging errors
  }
}

template <LoggerTrait T>
inline void Logger::SetLevel(T /*logger*/, LogLevel level) noexcept {
  SetLevelImpl(LoggerIdOf<T>(), level);
}

inline void Logger::SetLevel(LogLevel level) noexcept {
  SetLevelImpl(LoggerIdOf<DefaultLogger>(), level);
}

inline void Logger::SetLevelImpl(LoggerId logger_id, LogLevel level) noexcept {
  const std::scoped_lock lock(loggers_mutex_);
  if (const auto it = loggers_.find(logger_id); it != loggers_.end() && it->second) {
    it->second->level = level;
  }
}

template <LoggerTrait T>
inline bool Logger::HasLogger(T /*logger*/) const noexcept {
  const std::shared_lock lock(loggers_mutex_);
  return loggers_.contains(LoggerIdOf<T>());
}

inline bool Logger::ShouldLog(LogLevel level) const noexcept {
  return ShouldLogImpl(LoggerIdOf<DefaultLogger>(), level);
}

template <LoggerTrait T>
inline bool Logger::ShouldLog(T /*logger*/, LogLevel level) const noexcept {
  return ShouldLogImpl(LoggerIdOf<T>(), level);
}

inline bool Logger::ShouldLogImpl(LoggerId logger_id, LogLevel level) const noexcept {
  const std::shared_lock lock(loggers_mutex_);
  if (const auto it = loggers_.find(logger_id); it != loggers_.end() && it->second) {
    return level >= it->second->level;
  }
  return false;
}

template <LoggerTrait T>
inline LogLevel Logger::GetLevel(T /*logger*/) const noexcept {
  return GetLevelImpl(LoggerIdOf<T>());
}

inline LogLevel Logger::GetLevel() const noexcept {
  return GetLevelImpl(LoggerIdOf<DefaultLogger>());
}

inline LogLevel Logger::GetLevelImpl(LoggerId logger_id) const noexcept {
  const std::shared_lock lock(loggers_mutex_);
  if (const auto it = loggers_.find(logger_id); it != loggers_.end() && it->second) {
    return it->second->level;
  }
  return LogLevel::kTrace;
}

inline std::string Logger::FormatLogFileName(std::string_view logger_name, std::string_view pattern) noexcept {
  try {
    std::string result(pattern);

    if (size_t pos = result.find("{name}"); pos != std::string::npos) {
      result.replace(pos, 6, logger_name);
    }

    if (size_t pos = result.find("{timestamp}"); pos != std::string::npos) {
      const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
      result.replace(pos, 11, timestamp.toStdString());
    }

    return result;
  } catch (...) {
    return std::string(logger_name) + ".log";
  }
}

inline std::string Logger::FormatLogMessage(const LoggerData& data, LogLevel level, const std::source_location& loc,
                                            std::string_view message) noexcept {
  try {
    const QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss.zzz");
    const std::string_view level_str = LogLevelToString(level);

    std::string result;
    result.reserve(256);
    result.append("[");
    result.append(timestamp.toStdString());
    result.append("] [");
    result.append(level_str);
    result.append("] ");
    result.append(data.name);
    result.append(": ");
    result.append(message);

    // Add source location for higher severity levels
    if (level >= data.config.source_location_level) {
      const std::string_view filename = details::GetFileName(loc.file_name());
      result.append(" [");
      result.append(filename);
      result.append(":");
      result.append(std::to_string(loc.line()));
      result.append("]");
    }

    // Add stack trace for critical levels
    if (level >= data.config.stack_trace_level) {
      result.append(CaptureStackTrace());
    }

    return result;
  } catch (...) {
    return std::string(message);
  }
}

inline void Logger::WriteToConsole(LogLevel level, std::string_view message) noexcept {
  try {
    switch (level) {
      case LogLevel::kTrace:
      case LogLevel::kDebug:
        qDebug().noquote() << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()));
        break;
      case LogLevel::kInfo:
        qInfo().noquote() << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()));
        break;
      case LogLevel::kWarn:
        qWarning().noquote() << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()));
        break;
      case LogLevel::kError:
      case LogLevel::kCritical:
        qCritical().noquote() << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size()));
        break;
    }
  } catch (...) {
    // Silently ignore console output errors
  }
}

inline void Logger::WriteToFile(LoggerData& data, std::string_view message) noexcept {
  if (!data.file_stream) {
    return;
  }

  try {
    const QMutexLocker lock(&data.file_mutex);
    *data.file_stream << QString::fromUtf8(message.data(), static_cast<qsizetype>(message.size())) << "\n";
  } catch (...) {
    // Silently ignore file output errors
  }
}

}  // namespace client

// Provide logger integration for assert.hpp
namespace client::details {

#if !defined(_MSC_VER)
inline void LogAssertionFailureViaLogger(std::string_view condition, const std::source_location& loc,
                                         std::string_view message) noexcept {
  Logger::GetInstance().LogAssertionFailure(condition, loc, message);
}
#endif

}  // namespace client::details

// ============================================================================
// Logging Macros
// ============================================================================

#ifdef CLIENT_DEBUG_MODE
#define CLIENT_DEBUG(...) \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kDebug, std::source_location::current(), __VA_ARGS__)

#define CLIENT_DEBUG_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kDebug, std::source_location::current(), \
                                             __VA_ARGS__)
#else
#define CLIENT_DEBUG(...) [[maybe_unused]] static constexpr auto CLIENT_ANONYMOUS_VAR(unused_debug) = 0
#define CLIENT_DEBUG_LOGGER(logger, ...) \
  [[maybe_unused]] static constexpr auto CLIENT_ANONYMOUS_VAR(unused_debug_logger) = 0
#endif

#if defined(CLIENT_ENABLE_ASSERTS)
#define CLIENT_TRACE(...) \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kTrace, std::source_location::current(), __VA_ARGS__)

#define CLIENT_TRACE_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kTrace, std::source_location::current(), \
                                             __VA_ARGS__)
#else
#define CLIENT_TRACE(...) [[maybe_unused]] static constexpr auto CLIENT_ANONYMOUS_VAR(unused_trace) = 0
#define CLIENT_TRACE_LOGGER(logger, ...) \
  [[maybe_unused]] static constexpr auto CLIENT_ANONYMOUS_VAR(unused_trace_logger) = 0
#endif

#define CLIENT_INFO(...) \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kInfo, std::source_location::current(), __VA_ARGS__)
#define CLIENT_WARN(...) \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kWarn, std::source_location::current(), __VA_ARGS__)
#define CLIENT_ERROR(...) \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kError, std::source_location::current(), __VA_ARGS__)
#define CLIENT_CRITICAL(...)                                                                                 \
  ::client::Logger::GetInstance().LogMessage(::client::LogLevel::kCritical, std::source_location::current(), \
                                             __VA_ARGS__)

#define CLIENT_INFO_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kInfo, std::source_location::current(), \
                                             __VA_ARGS__)
#define CLIENT_WARN_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kWarn, std::source_location::current(), \
                                             __VA_ARGS__)
#define CLIENT_ERROR_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kError, std::source_location::current(), \
                                             __VA_ARGS__)
#define CLIENT_CRITICAL_LOGGER(logger, ...)                                                                          \
  ::client::Logger::GetInstance().LogMessage(logger, ::client::LogLevel::kCritical, std::source_location::current(), \
                                             __VA_ARGS__)

// Keep compatibility with CLIENT_CORE_* macros for internal core usage
#define CLIENT_CORE_TRACE(...) CLIENT_TRACE(__VA_ARGS__)
#define CLIENT_CORE_DEBUG(...) CLIENT_DEBUG(__VA_ARGS__)
#define CLIENT_CORE_INFO(...) CLIENT_INFO(__VA_ARGS__)
#define CLIENT_CORE_WARN(...) CLIENT_WARN(__VA_ARGS__)
#define CLIENT_CORE_ERROR(...) CLIENT_ERROR(__VA_ARGS__)
#define CLIENT_CORE_CRITICAL(...) CLIENT_CRITICAL(__VA_ARGS__)
