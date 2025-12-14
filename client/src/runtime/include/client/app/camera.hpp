#pragma once

#include <client/pch.hpp>

#include <client/app/frame.hpp>

#include <QCamera>
#include <QCameraDevice>
#include <QMediaCaptureSession>
#include <QVideoSink>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

class QVideoSink;

namespace client {

/**
 * @brief Error codes for camera operations.
 */
enum class CameraError : uint8_t {
  kNotFound,           ///< No camera device found.
  kAccessDenied,       ///< Camera access denied.
  kAlreadyInUse,       ///< Camera is already in use.
  kNotStarted,         ///< Camera not started.
  kInvalidDevice,      ///< Invalid camera device.
  kCaptureError,       ///< Error during frame capture.
  kConfigurationError  ///< Camera configuration error.
};

/**
 * @brief Converts CameraError to a human-readable string.
 * @param error The error to convert.
 * @return A string view representing the error.
 */
[[nodiscard]] constexpr std::string_view CameraErrorToString(CameraError error) noexcept {
  switch (error) {
    case CameraError::kNotFound:
      return "Camera not found";
    case CameraError::kAccessDenied:
      return "Camera access denied";
    case CameraError::kAlreadyInUse:
      return "Camera already in use";
    case CameraError::kNotStarted:
      return "Camera not started";
    case CameraError::kInvalidDevice:
      return "Invalid camera device";
    case CameraError::kCaptureError:
      return "Frame capture error";
    case CameraError::kConfigurationError:
      return "Camera configuration error";
  }
  return "Unknown error";
}

/**
 * @brief Information about a camera device.
 */
struct CameraDeviceInfo {
  std::string id;           ///< Unique device identifier.
  std::string description;  ///< Human-readable description.
  bool is_default = false;  ///< Whether this is the default camera.
};

/**
 * @brief Configuration for the camera.
 */
struct CameraConfig {
  std::string device_id;          ///< Device ID to use (empty for default).
  int preferred_width = 640;      ///< Preferred capture width.
  int preferred_height = 480;     ///< Preferred capture height.
  int preferred_fps = 30;         ///< Preferred frames per second (also used for throttling).
  bool enable_throttling = true;  ///< Enable software frame rate throttling.
};

/**
 * @brief Camera access wrapper using Qt Multimedia.
 * @details Provides a simplified interface for camera access with
 * automatic format conversion, frame callback support, and software
 * frame rate throttling to ensure consistent frame rates regardless
 * of camera hardware capabilities.
 */
class Camera final : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Callback type for receiving new frames.
   */
#if defined(__cpp_lib_move_only_function)
  using FrameCallback = std::move_only_function<void(const Frame&)>;
#else
  using FrameCallback = std::function<void(const Frame&)>;
#endif

  /**
   * @brief Constructs an uninitialized camera.
   * @param parent Optional parent QObject.
   */
  explicit Camera(QObject* parent = nullptr) : QObject(parent) {}

  Camera(const Camera&) = delete;
  Camera(Camera&&) = delete;
  ~Camera() override { Stop(); }

  Camera& operator=(const Camera&) = delete;
  Camera& operator=(Camera&&) = delete;

  /**
   * @brief Lists all available camera devices.
   * @return Vector of available camera devices.
   */
  [[nodiscard]] static auto AvailableDevices() -> std::vector<CameraDeviceInfo>;

  /**
   * @brief Gets the number of available camera devices.
   * @return Number of available cameras.
   */
  [[nodiscard]] static size_t AvailableDeviceCount() noexcept;

  /**
   * @brief Gets the default camera device info.
   * @return Default camera info, or nullopt if no cameras available.
   */
  [[nodiscard]] static auto DefaultDevice() -> std::optional<CameraDeviceInfo>;

  /**
   * @brief Initializes the camera with the given configuration.
   * @param config Camera configuration.
   * @return Expected void on success, or CameraError on failure.
   */
  [[nodiscard]] auto Initialize(const CameraConfig& config = {}) -> std::expected<void, CameraError>;

  /**
   * @brief Starts the camera capture.
   * @return Expected void on success, or CameraError on failure.
   */
  [[nodiscard]] auto Start() -> std::expected<void, CameraError>;

  /**
   * @brief Stops the camera capture.
   */
  void Stop();

  /**
   * @brief Switches to a different camera device.
   * @param device_id The ID of the camera to switch to (empty for default).
   * @return Expected void on success, or CameraError on failure.
   * @warning Camera must be initialized before switching.
   */
  [[nodiscard]] auto SwitchCamera(std::string_view device_id) -> std::expected<void, CameraError>;

  /**
   * @brief Switches to the next available camera device.
   * @return Expected void on success, or CameraError on failure.
   * @warning Camera must be initialized before switching. If at last camera, wraps to first.
   */
  [[nodiscard]] auto SwitchToNextCamera() -> std::expected<void, CameraError>;

  /**
   * @brief Switches to the previous available camera device.
   * @return Expected void on success, or CameraError on failure.
   * @warning Camera must be initialized before switching. If at first camera, wraps to last.
   */
  [[nodiscard]] auto SwitchToPreviousCamera() -> std::expected<void, CameraError>;

  /**
   * @brief Captures a single frame synchronously.
   * @return Expected Frame on success, or CameraError on failure.
   */
  [[nodiscard]] auto CaptureFrame() -> std::expected<Frame, CameraError>;

  /**
   * @brief Updates the camera configuration at runtime.
   * @param new_config New camera configuration to apply.
   * @details Updates FPS, throttling, and resolution preferences.
   * Resolution changes require camera restart to take effect.
   */
  void UpdateConfig(const CameraConfig& new_config) noexcept;

  /**
   * @brief Sets the callback for receiving new frames.
   * @param callback The callback function to invoke on each new frame.
   */
  void SetFrameCallback(FrameCallback callback) noexcept { frame_callback_ = std::move(callback); }

  /**
   * @brief Sets the target frame rate for software throttling.
   * @details This allows runtime adjustment of the frame rate limit.
   * @param fps Target frames per second (must be positive).
   * @warning Asserts if fps <= 0.
   */
  void TargetFps(int fps) noexcept;

  /**
   * @brief Enables or disables software frame rate throttling.
   * @param enabled True to enable throttling.
   */
  void ThrottlingEnabled(bool enabled) noexcept { config_.enable_throttling = enabled; }

  /**
   * @brief Checks if the camera is currently active.
   * @return True if capturing.
   */
  [[nodiscard]] bool Active() const noexcept { return active_.load(std::memory_order_acquire); }

  /**
   * @brief Checks if the camera is initialized.
   * @return True if initialized.
   */
  [[nodiscard]] bool Initialized() const noexcept { return initialized_.load(std::memory_order_acquire); }

  /**
   * @brief Checks if throttling is enabled.
   * @return True if throttling is enabled.
   */
  [[nodiscard]] bool ThrottlingEnabled() const noexcept { return config_.enable_throttling; }

  /**
   * @brief Gets the target FPS for throttling.
   * @return Target FPS.
   */
  [[nodiscard]] int TargetFps() const noexcept { return config_.preferred_fps; }

  /**
   * @brief Gets the current camera configuration.
   * @return Reference to the current configuration.
   */
  [[nodiscard]] const CameraConfig& Config() const noexcept { return config_; }

  /**
   * @brief Gets the actual capture width.
   * @return Capture width in pixels.
   */
  [[nodiscard]] int CaptureWidth() const noexcept { return capture_width_; }

  /**
   * @brief Gets the actual capture height.
   * @return Capture height in pixels.
   */
  [[nodiscard]] int CaptureHeight() const noexcept { return capture_height_; }

  /**
   * @brief Gets the total number of frames captured.
   * @return Frame count.
   */
  [[nodiscard]] uint64_t FramesCaptured() const noexcept { return frames_captured_.load(std::memory_order_relaxed); }

  /**
   * @brief Gets the number of frames dropped due to throttling.
   * @return Dropped frame count.
   */
  [[nodiscard]] uint64_t FramesDropped() const noexcept { return frames_dropped_.load(std::memory_order_relaxed); }

  /**
   * @brief Gets the current camera device information.
   * @return Current camera device info, or nullopt if not initialized.
   */
  [[nodiscard]] auto CurrentDevice() const noexcept -> std::optional<CameraDeviceInfo>;

signals:
  /**
   * @brief Signal emitted when a new frame is available.
   * @param frame The captured frame.
   */
  void FrameReady(const Frame& frame);

  /**
   * @brief Signal emitted when a camera error occurs.
   * @param error The error that occurred.
   */
  void ErrorOccurred(CameraError error);

private slots:
  /**
   * @brief Handles new video frames from the video sink.
   * @param frame The Qt video frame.
   */
  void OnVideoFrameChanged(const QVideoFrame& frame);

  /**
   * @brief Handles camera error signals.
   * @param error The Qt camera error.
   */
  void OnCameraError(QCamera::Error error);

private:
  /**
   * @brief Checks if enough time has passed to emit the next frame.
   * @return True if the frame should be processed, false if dropped.
   */
  [[nodiscard]] bool ShouldProcessFrame() noexcept;

  /**
   * @brief Converts a QVideoFrame to our Frame type.
   * @param qframe The Qt video frame.
   * @return Converted Frame.
   */
  [[nodiscard]] Frame ConvertFrame(const QVideoFrame& qframe);

  /**
   * @brief Finds a camera device by ID.
   * @param device_id The device ID to find (empty for default).
   * @return The camera device, or nullopt if not found.
   */
  [[nodiscard]] static auto FindDevice(std::string_view device_id) -> std::optional<QCameraDevice>;

  // Note: Qt objects (QCamera, QMediaCaptureSession, QVideoSink) are QObject-derived
  // and have deleted copy/move constructors. They must be heap-allocated.
  // Using unique_ptr ensures proper RAII and ownership semantics.
  std::unique_ptr<QCamera> camera_;
  std::unique_ptr<QMediaCaptureSession> capture_session_;
  std::unique_ptr<QVideoSink> video_sink_;

  CameraConfig config_;
  FrameCallback frame_callback_;
  Frame last_frame_;

  // Frame rate throttling (protected by throttle_mutex_)
  mutable std::mutex throttle_mutex_;
  std::chrono::steady_clock::time_point last_frame_time_{};
  std::chrono::microseconds frame_interval_{33333};  ///< Default: ~30 FPS

  std::atomic<uint64_t> frames_captured_{0};
  std::atomic<uint64_t> frames_dropped_{0};
  std::atomic<int> capture_width_{0};
  std::atomic<int> capture_height_{0};

  std::atomic<bool> initialized_{false};
  std::atomic<bool> active_{false};
};

}  // namespace client
