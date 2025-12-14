#include <client/core/logger.hpp>

#include <cstddef>
#include <source_location>
#include <string>
#include <string_view>

#ifdef CLIENT_ENABLE_STACKTRACE
// CLIENT_USE_STD_STACKTRACE is defined by CMake when std::stacktrace is available
// We don't rely on __cpp_lib_stacktrace because it may be defined in headers
// even when the linker symbols are not available (e.g., Clang with libstdc++ headers)
#ifdef CLIENT_USE_STD_STACKTRACE
#include <stacktrace>
#else
#include <boost/stacktrace.hpp>
#endif
#endif

namespace client {

/**
 * @brief Captures a stack trace as a string.
 * @return Stack trace string or message indicating unavailability.
 */
std::string Logger::CaptureStackTrace() noexcept {
#ifdef CLIENT_ENABLE_STACKTRACE
  constexpr size_t kMaxStackTraceFrames = 10;

  try {
    std::string result;
    result.reserve(512);
    result.append("\nStack trace:");

#ifdef CLIENT_USE_STD_STACKTRACE
    const auto stack_trace = std::stacktrace::current();
    const auto stack_size = static_cast<size_t>(stack_trace.size());
    if (stack_size <= 1) {
      result.append(" <empty>");
      return result;
    }

    const size_t frame_count = std::min(stack_size, 1 + kMaxStackTraceFrames);
    for (size_t i = 1, out_idx = 1; i < frame_count; ++i, ++out_idx) {
      const auto& entry = stack_trace[static_cast<std::stacktrace::size_type>(i)];
      result.append("\n  ");
      result.append(std::to_string(out_idx));
      result.append(": ");
      result.append(std::to_string(entry));
    }
#else
    const boost::stacktrace::stacktrace stack_trace;
    if (stack_trace.size() <= 1) {
      result.append(" <empty>");
      return result;
    }

    const size_t frame_count = std::min(stack_trace.size(), 1 + kMaxStackTraceFrames);
    for (size_t i = 1, out_idx = 1; i < frame_count; ++i, ++out_idx) {
      const auto& entry = stack_trace[i];
      result.append("\n  ");
      result.append(std::to_string(out_idx));
      result.append(": ");
      result.append(boost::stacktrace::to_string(entry));
    }
#endif

    return result;
  } catch (...) {
    return "\nStack trace: <error during capture>";
  }
#else
  return "";
#endif
}

}  // namespace client

// MSVC-specific definition for assertion logging integration
// On GCC/Clang, the inline definition in logger.hpp takes precedence over the weak symbol in assert.cpp
#if defined(_MSC_VER)
namespace client::details {

void LogAssertionFailureViaLogger(std::string_view condition, const std::source_location& loc,
                                  std::string_view message) noexcept {
  Logger::GetInstance().LogAssertionFailure(condition, loc, message);
}

}  // namespace client::details
#endif
