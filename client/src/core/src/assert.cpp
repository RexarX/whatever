#include <client/core/assert.hpp>

#include <client/core/core.hpp>

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string>
#include <string_view>

#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
#include <print>
#endif

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

namespace {

#ifdef CLIENT_ENABLE_STACKTRACE
[[nodiscard]] std::string CaptureStackTrace() noexcept {
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
}
#endif

}  // namespace

namespace client {

void AbortWithStacktrace(std::string_view message) noexcept {
#if defined(__cpp_lib_print) && (__cpp_lib_print >= 202302L)
  // Use std::println when available (C++23)
  std::println(stderr, "\n=== FATAL ERROR ===");
  std::println(stderr, "Message: {}", message);

#ifdef CLIENT_ENABLE_STACKTRACE
  std::println(stderr, CaptureStackTrace);
#else
  std::println(stderr, "\nStack trace: <not available - build with CLIENT_ENABLE_STACKTRACE>");
#endif

  std::println(stderr, "===================\n");
  std::fflush(stderr);
#else
  // Fallback to fprintf to stderr when std::println is not available
  std::fprintf(stderr, "\n=== FATAL ERROR ===\n");
  std::fprintf(stderr, "Message: %.*s\n", static_cast<int>(message.size()), message.data());

#ifdef CLIENT_ENABLE_STACKTRACE
  std::fprintf(stderr, "%s\n", CaptureStackTrace().c_str());
#else
  std::fprintf(stderr, "\nStack trace: <not available - build with HELIOS_ENABLE_STACKTRACE>\n");
#endif

  std::fprintf(stderr, "===================\n\n");
  std::fflush(stderr);
#endif

  CLIENT_DEBUG_BREAK();
  std::abort();
}

namespace details {

// Weak symbol stub - will be overridden by logger.hpp inline version if included
// On MSVC, the definition is provided by logger.cpp, so we skip this fallback
// On GCC/Clang, we use weak attribute so the inline version from logger.hpp takes precedence
#if !defined(_MSC_VER)
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
void LogAssertionFailureViaLogger([[maybe_unused]] std::string_view condition,
                                  [[maybe_unused]] const std::source_location& loc,
                                  [[maybe_unused]] std::string_view message) noexcept {
  // Default implementation does nothing.
  // The real implementation is provided by logger.hpp when included.
}
#endif

}  // namespace details

}  // namespace client
