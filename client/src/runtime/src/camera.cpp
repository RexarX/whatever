#include <client/app/camera.hpp>

#include <client/core/assert.hpp>
#include <client/core/logger.hpp>

#include <QCamera>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QObject>
#include <QVideoFrame>
#include <QVideoSink>

#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cstddef>
#include <exception>
#include <expected>
#include <limits>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace client {

auto Camera::AvailableDevices() -> std::vector<CameraDeviceInfo> {
  std::vector<CameraDeviceInfo> devices;
  const auto qt_devices = QMediaDevices::videoInputs();
  const auto default_device = QMediaDevices::defaultVideoInput();

  devices.reserve(static_cast<size_t>(qt_devices.size()));

  for (const auto& device : qt_devices) {
    CameraDeviceInfo info;
    info.id = device.id().toStdString();
    info.description = device.description().toStdString();
    info.is_default = (device == default_device);
    devices.push_back(std::move(info));
  }

  return devices;
}

auto Camera::DefaultDevice() -> std::optional<CameraDeviceInfo> {
  const auto device = QMediaDevices::defaultVideoInput();
  if (device.isNull()) {
    return std::nullopt;
  }

  CameraDeviceInfo info;
  info.id = device.id().toStdString();
  info.description = device.description().toStdString();
  info.is_default = true;
  return info;
}

auto Camera::Initialize(const CameraConfig& config) -> std::expected<void, CameraError> {
  if (initialized_.load(std::memory_order_acquire)) {
    Stop();
  }

  config_ = config;

  // Calculate frame interval for throttling
  std::chrono::microseconds new_interval;
  if (config_.preferred_fps > 0) {
    new_interval = std::chrono::microseconds(1'000'000 / config_.preferred_fps);
  } else {
    new_interval = std::chrono::microseconds(33333);  // Default 30 FPS
  }

  {
    std::scoped_lock lock(throttle_mutex_);
    frame_interval_ = new_interval;
  }

  // Find the camera device
  auto device = FindDevice(config_.device_id);
  if (!device) {
    CLIENT_ERROR("Camera device not found: {}", config_.device_id.empty() ? "default" : config_.device_id);
    return std::unexpected(CameraError::kNotFound);
  }

  try {
    // Create camera
    camera_ = std::make_unique<QCamera>(*device);

    // Create video sink
    video_sink_ = std::make_unique<QVideoSink>();

    // Create capture session
    capture_session_ = std::make_unique<QMediaCaptureSession>();
    capture_session_->setCamera(camera_.get());
    capture_session_->setVideoSink(video_sink_.get());

    // Connect signals
    connect(video_sink_.get(), &QVideoSink::videoFrameChanged, this, &Camera::OnVideoFrameChanged);
    connect(camera_.get(), &QCamera::errorOccurred, this, &Camera::OnCameraError);

    // Configure camera format if possible
    auto formats = device->videoFormats();
    if (!formats.isEmpty()) {
      // Find a format closest to the preferred settings
      QCameraFormat best_format = formats.first();
      int best_score = std::numeric_limits<int>::max();

      for (const auto& format : formats) {
        const int width_diff = std::abs(format.resolution().width() - config_.preferred_width);
        const int height_diff = std::abs(format.resolution().height() - config_.preferred_height);
        const int fps_diff = std::abs(static_cast<int>(format.maxFrameRate()) - config_.preferred_fps);
        const int score = width_diff + height_diff + fps_diff * 10;

        if (score < best_score) {
          best_score = score;
          best_format = format;
        }
      }

      camera_->setCameraFormat(best_format);
      const int width = best_format.resolution().width();
      const int height = best_format.resolution().height();
      capture_width_.store(width, std::memory_order_relaxed);
      capture_height_.store(height, std::memory_order_relaxed);

      CLIENT_INFO("Camera configured: {}x{} @ {} fps (hardware), throttle target: {} fps", width, height,
                  static_cast<int>(best_format.maxFrameRate()), config_.preferred_fps);
    }

    // Reset throttling state
    {
      std::scoped_lock lock(throttle_mutex_);
      last_frame_time_ = std::chrono::steady_clock::time_point{};
    }
    frames_dropped_.store(0, std::memory_order_relaxed);

    initialized_.store(true, std::memory_order_release);
    CLIENT_INFO("Camera initialized: {} (throttling: {})", device->description().toStdString(),
                config_.enable_throttling ? "enabled" : "disabled");

    return {};
  } catch (const std::exception& e) {
    CLIENT_ERROR("Failed to initialize camera: {}", e.what());
    return std::unexpected(CameraError::kConfigurationError);
  }
}

auto Camera::Start() -> std::expected<void, CameraError> {
  if (!initialized_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot start: camera not initialized");
    return std::unexpected(CameraError::kNotStarted);
  }

  if (active_.load(std::memory_order_acquire)) {
    return {};  // Already running
  }

  // Reset frame timing when starting
  {
    std::scoped_lock lock(throttle_mutex_);
    last_frame_time_ = std::chrono::steady_clock::now();
  }

  camera_->start();
  active_.store(true, std::memory_order_release);
  CLIENT_INFO("Camera started");

  return {};
}

void Camera::Stop() {
  if (!active_.load(std::memory_order_acquire)) {
    return;
  }

  if (camera_) [[likely]] {
    camera_->stop();
  }

  active_.store(false, std::memory_order_release);
  CLIENT_INFO("Camera stopped (captured: {}, dropped: {})", frames_captured_.load(std::memory_order_relaxed),
              frames_dropped_.load(std::memory_order_relaxed));
}

auto Camera::SwitchCamera(std::string_view device_id) -> std::expected<void, CameraError> {
  if (!initialized_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot switch camera: not initialized");
    return std::unexpected(CameraError::kNotStarted);
  }

  // Find the new device
  auto device = FindDevice(device_id);
  if (!device) {
    CLIENT_ERROR("Camera device not found: {}", device_id.empty() ? "default" : device_id);
    return std::unexpected(CameraError::kNotFound);
  }

  // Remember if we were active
  const bool was_active = active_.load(std::memory_order_acquire);

  // Stop current camera
  if (was_active) {
    Stop();
  }

  // Update config
  config_.device_id = std::string(device_id);

  try {
    // Recreate camera with new device
    camera_ = std::make_unique<QCamera>(*device);

    // Reconnect to existing session
    capture_session_->setCamera(camera_.get());

    // Reconnect signals
    connect(camera_.get(), &QCamera::errorOccurred, this, &Camera::OnCameraError);

    // Configure camera format if possible
    auto formats = device->videoFormats();
    if (!formats.isEmpty()) {
      QCameraFormat best_format = formats.first();
      int best_score = std::numeric_limits<int>::max();

      for (const auto& format : formats) {
        const int width_diff = std::abs(format.resolution().width() - config_.preferred_width);
        const int height_diff = std::abs(format.resolution().height() - config_.preferred_height);
        const int fps_diff = std::abs(static_cast<int>(format.maxFrameRate()) - config_.preferred_fps);
        const int score = width_diff + height_diff + fps_diff * 10;

        if (score < best_score) {
          best_score = score;
          best_format = format;
        }
      }

      camera_->setCameraFormat(best_format);
      const int width = best_format.resolution().width();
      const int height = best_format.resolution().height();
      capture_width_.store(width, std::memory_order_relaxed);
      capture_height_.store(height, std::memory_order_relaxed);

      CLIENT_INFO("Switched camera: {}x{} @ {} fps (hardware), throttle target: {} fps", width, height,
                  static_cast<int>(best_format.maxFrameRate()), config_.preferred_fps);
    }

    // Reset throttling state
    {
      std::scoped_lock lock(throttle_mutex_);
      last_frame_time_ = std::chrono::steady_clock::time_point{};
    }

    CLIENT_INFO("Camera switched to: {}", device->description().toStdString());

    // Restart if we were active
    if (was_active) {
      return Start();
    }

    return {};
  } catch (const std::exception& e) {
    CLIENT_ERROR("Failed to switch camera: {}", e.what());
    return std::unexpected(CameraError::kConfigurationError);
  }
}

auto Camera::SwitchToNextCamera() -> std::expected<void, CameraError> {
  if (!initialized_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot switch camera: not initialized");
    return std::unexpected(CameraError::kNotStarted);
  }

  const auto devices = QMediaDevices::videoInputs();
  if (devices.isEmpty()) {
    return std::unexpected(CameraError::kNotFound);
  }

  // Find current device index
  const std::string current_id = config_.device_id;
  int current_index = -1;

  for (int i = 0; i < devices.size(); ++i) {
    if (devices[i].id().toStdString() == current_id) {
      current_index = i;
      break;
    }
  }

  // If current device not found or we're at the end, wrap to first
  const int next_index = (current_index + 1) % static_cast<int>(devices.size());
  const std::string next_id = devices[next_index].id().toStdString();

  return SwitchCamera(next_id);
}

auto Camera::SwitchToPreviousCamera() -> std::expected<void, CameraError> {
  if (!initialized_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot switch camera: not initialized");
    return std::unexpected(CameraError::kNotStarted);
  }

  const auto devices = QMediaDevices::videoInputs();
  if (devices.isEmpty()) {
    return std::unexpected(CameraError::kNotFound);
  }

  // Find current device index
  const std::string current_id = config_.device_id;
  int current_index = -1;

  for (int i = 0; i < devices.size(); ++i) {
    if (devices[i].id().toStdString() == current_id) {
      current_index = i;
      break;
    }
  }

  // If current device not found, start from first
  if (current_index == -1) {
    current_index = 0;
  }

  // Wrap to last if we're at the first
  const int prev_index = (current_index - 1 + static_cast<int>(devices.size())) % static_cast<int>(devices.size());
  const std::string prev_id = devices[prev_index].id().toStdString();

  return SwitchCamera(prev_id);
}

auto Camera::CaptureFrame() -> std::expected<Frame, CameraError> {
  if (!initialized_.load(std::memory_order_acquire)) {
    return std::unexpected(CameraError::kNotStarted);
  }

  if (!active_.load(std::memory_order_acquire)) {
    return std::unexpected(CameraError::kNotStarted);
  }

  if (last_frame_.Empty()) {
    return std::unexpected(CameraError::kCaptureError);
  }

  return last_frame_.Clone();
}

void Camera::UpdateConfig(const CameraConfig& new_config) noexcept {
  std::scoped_lock lock(throttle_mutex_);

  // Update FPS if changed
  if (config_.preferred_fps != new_config.preferred_fps) {
    config_.preferred_fps = new_config.preferred_fps;
    frame_interval_ = std::chrono::microseconds(1'000'000 / config_.preferred_fps);
    CLIENT_INFO("Camera target FPS updated to: {}", config_.preferred_fps);
  }

  // Update throttling
  if (config_.enable_throttling != new_config.enable_throttling) {
    config_.enable_throttling = new_config.enable_throttling;
    CLIENT_INFO("Frame throttling {}", config_.enable_throttling ? "enabled" : "disabled");
  }

  // Update resolution preferences (requires restart to take effect)
  if (config_.preferred_width != new_config.preferred_width ||
      config_.preferred_height != new_config.preferred_height) {
    config_.preferred_width = new_config.preferred_width;
    config_.preferred_height = new_config.preferred_height;
    CLIENT_INFO("Camera resolution preferences updated to: {}x{} (restart required)", config_.preferred_width,
                config_.preferred_height);
  }

  CLIENT_INFO("Camera config updated: {}x{} @ {} fps, throttling: {}", config_.preferred_width,
              config_.preferred_height, config_.preferred_fps, config_.enable_throttling ? "ON" : "OFF");
}

void Camera::TargetFps(int fps) noexcept {
  CLIENT_ASSERT(fps > 0, "FPS must be positive");
  config_.preferred_fps = fps;

  {
    std::scoped_lock lock(throttle_mutex_);
    frame_interval_ = std::chrono::microseconds(1'000'000 / fps);
  }

  CLIENT_INFO("Camera target FPS set to: {}", fps);
}

bool Camera::ShouldProcessFrame() noexcept {
  if (!config_.enable_throttling) {
    return true;
  }

  std::scoped_lock lock(throttle_mutex_);
  const auto now = std::chrono::steady_clock::now();

  // First frame always processed
  if (last_frame_time_.time_since_epoch().count() == 0) {
    last_frame_time_ = now;
    return true;
  }

  const auto elapsed = now - last_frame_time_;

  if (elapsed >= frame_interval_) {
    last_frame_time_ = now;
    return true;
  }

  // Frame dropped due to throttling
  frames_dropped_.fetch_add(1, std::memory_order_relaxed);
  return false;
}

void Camera::OnVideoFrameChanged(const QVideoFrame& frame) {
  if (!frame.isValid()) [[unlikely]] {
    return;
  }

  // Check if we should process this frame (throttling)
  if (!ShouldProcessFrame()) {
    return;
  }

  Frame converted = ConvertFrame(frame);
  if (converted.Empty()) {
    return;
  }

  last_frame_ = std::move(converted);
  frames_captured_.fetch_add(1, std::memory_order_relaxed);

  // Emit signal
  emit FrameReady(last_frame_);

  // Call callback if set
  if (frame_callback_) {
    frame_callback_(last_frame_);
  }
}

void Camera::OnCameraError(QCamera::Error error) {
  CameraError camera_error;

  switch (error) {
    case QCamera::NoError:
      return;
    case QCamera::CameraError:
      camera_error = CameraError::kCaptureError;
      break;
    default:
      camera_error = CameraError::kCaptureError;
      break;
  }

  CLIENT_ERROR("Camera error: {}", CameraErrorToString(camera_error));
  emit ErrorOccurred(camera_error);
}

Frame Camera::ConvertFrame(const QVideoFrame& qframe) {
  QVideoFrame frame_copy = qframe;

  if (!frame_copy.map(QVideoFrame::ReadOnly)) {
    CLIENT_WARN("Failed to map video frame");
    return {};
  }

  // Get frame dimensions
  const int width = frame_copy.width();
  const int height = frame_copy.height();

  // Update capture dimensions if they changed
  const int old_width = capture_width_.load(std::memory_order_relaxed);
  const int old_height = capture_height_.load(std::memory_order_relaxed);
  if (old_width != width || old_height != height) {
    capture_width_.store(width, std::memory_order_relaxed);
    capture_height_.store(height, std::memory_order_relaxed);
  }

  // Convert to QImage first
  QImage image = frame_copy.toImage();
  frame_copy.unmap();

  if (image.isNull()) {
    CLIENT_WARN("Failed to convert video frame to image");
    return {};
  }

  // Convert to BGR format for OpenCV
  image = image.convertToFormat(QImage::Format_RGB888);

  // Create OpenCV Mat from QImage
  cv::Mat mat(image.height(), image.width(), CV_8UC3, image.bits(), static_cast<size_t>(image.bytesPerLine()));

  // Clone the mat since QImage will be destroyed
  Frame result(mat.clone());

  // Convert RGB to BGR for OpenCV
  cv::cvtColor(result.Mat(), result.Mat(), cv::COLOR_RGB2BGR);

  return result;
}

auto Camera::CurrentDevice() const noexcept -> std::optional<CameraDeviceInfo> {
  if (!initialized_ || !camera_) {
    return std::nullopt;
  }

  const auto device = camera_->cameraDevice();
  if (device.isNull()) {
    return std::nullopt;
  }

  CameraDeviceInfo info;
  info.id = device.id().toStdString();
  info.description = device.description().toStdString();
  info.is_default = (device == QMediaDevices::defaultVideoInput());
  return info;
}

size_t Camera::AvailableDeviceCount() noexcept {
  return static_cast<size_t>(QMediaDevices::videoInputs().size());
}

auto Camera::FindDevice(std::string_view device_id) -> std::optional<QCameraDevice> {
  const auto devices = QMediaDevices::videoInputs();

  if (device_id.empty()) {
    // Return default device
    auto default_device = QMediaDevices::defaultVideoInput();
    if (!default_device.isNull()) {
      return default_device;
    }
    // Fall back to first available device
    if (!devices.isEmpty()) {
      return devices.first();
    }
    return std::nullopt;
  }

  // Find device by ID
  for (const auto& device : devices) {
    if (device.id().toStdString() == device_id) {
      return device;
    }
  }

  return std::nullopt;
}

}  // namespace client
