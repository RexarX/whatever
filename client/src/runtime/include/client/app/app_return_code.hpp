#pragma once

#include <client/pch.hpp>

#include <cstdint>
#include <string_view>

namespace client {

/**
 * @brief Application return codes indicating execution result.
 */
enum class AppReturnCode : uint8_t {
  kSuccess = 0,            ///< Application executed successfully.
  kCameraInitFailed,       ///< Failed to initialize camera.
  kFaceTrackerInitFailed,  ///< Failed to initialize face tracker.
  kModelLoadFailed,        ///< Failed to load neural network model.
  kFrameCaptureError,      ///< Error during frame capture.
  kInvalidConfiguration,   ///< Invalid application configuration.
  kUnknownError = 255      ///< Unknown error occurred.
};

/**
 * @brief Converts AppReturnCode to a human-readable string.
 * @param code The return code to convert.
 * @return A string view representing the return code.
 */
[[nodiscard]] constexpr std::string_view AppReturnCodeToString(AppReturnCode code) noexcept {
  switch (code) {
    case AppReturnCode::kSuccess:
      return "Success";
    case AppReturnCode::kCameraInitFailed:
      return "Camera initialization failed";
    case AppReturnCode::kFaceTrackerInitFailed:
      return "Face tracker initialization failed";
    case AppReturnCode::kModelLoadFailed:
      return "Model load failed";
    case AppReturnCode::kFrameCaptureError:
      return "Frame capture error";
    case AppReturnCode::kInvalidConfiguration:
      return "Invalid configuration";
    case AppReturnCode::kUnknownError:
      return "Unknown error";
  }
  return "Unknown error";
}

/**
 * @brief Checks if the return code indicates success.
 * @param code The return code to check.
 * @return True if the code indicates success.
 */
[[nodiscard]] constexpr bool IsSuccess(AppReturnCode code) noexcept {
  return code == AppReturnCode::kSuccess;
}

/**
 * @brief Converts AppReturnCode to an integer exit code.
 * @param code The return code to convert.
 * @return Integer exit code suitable for returning from main().
 */
[[nodiscard]] constexpr int ToExitCode(AppReturnCode code) noexcept {
  return static_cast<int>(code);
}

}  // namespace client
