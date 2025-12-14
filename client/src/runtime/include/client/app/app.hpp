#pragma once

#include <client/pch.hpp>

#include <client/app/app_return_code.hpp>
#include <client/app/camera.hpp>
#include <client/app/face_tracker.hpp>
#include <client/app/model_config.hpp>
#include <client/comm/bluetooth.hpp>
#include <client/core/logger.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

// Check for std::move_only_function availability (C++23 feature)
// Android NDK's libc++ doesn't support it yet
#if defined(__cpp_lib_move_only_function) && __cpp_lib_move_only_function >= 202110L
#define CLIENT_HAS_MOVE_ONLY_FUNCTION 1
#else
#define CLIENT_HAS_MOVE_ONLY_FUNCTION 0
#endif

class QCoreApplication;

namespace client {

// Forward declarations
class GuiWindow;

namespace comm {

class BluetoothManager;

}

/**
 * @brief Application configuration.
 */
struct AppConfig {
  CameraConfig camera;                           ///< Camera configuration.
  FaceTrackerConfig face_tracker;                ///< Face tracker configuration.
  ModelType model_type = ModelType::kYuNetONNX;  ///< Selected model type.
  bool headless = false;                         ///< Run without GUI.
  bool verbose = false;                          ///< Enable verbose logging.
  uint32_t max_frames = 0;                       ///< Maximum frames to process (0 = unlimited).

  /**
   * @brief Gets the default application configuration.
   * @return Default AppConfig instance
   */
  [[nodiscard]] static AppConfig Default();
};

/**
 * @brief Parses command line arguments into configuration.
 * @param argc Argument count
 * @param argv Argument values
 * @return Parsed configuration
 */
[[nodiscard]] AppConfig ParseArguments(int argc, char** argv);

/**
 * @brief Makes embedded Android model files available via real filesystem paths.
 * @details On Android, model files are packaged into the APK as Qt resources (e.g. `:/models/...`).
 * OpenCV cannot load models directly from Qt resources, so the files must be extracted to a real
 * filesystem location first (typically under `QStandardPaths::AppDataLocation`),
 * then `config` is rewritten to point to those extracted files.
 *
 * On non-Android platforms (or when model/config paths are already absolute),
 * this function does nothing and returns true.
 *
 * @param config Application configuration to update in-place
 * @return True on success (or when no action is needed); false on failure
 */
[[nodiscard]] bool ResolveEmbeddedModelsIfNeeded(AppConfig& config) noexcept;

inline AppConfig AppConfig::Default() {
  AppConfig config;

  // Camera defaults
  config.camera.preferred_width = 640;
  config.camera.preferred_height = 480;
  config.camera.preferred_fps = 30;

  // Use default model configuration (YuNet ONNX)
  const auto model_config = ModelConfig::Default();
  config.face_tracker = FaceTrackerConfig::FromModelConfig(model_config);
  config.model_type = ModelType::kYuNetONNX;

  config.headless = false;
  config.verbose = false;
  config.max_frames = 0;

  return config;
}

/**
 * @brief Main application class that orchestrates camera and face tracking.
 * @details Holds all application state and provides a simple Run() interface
 * for executing the main application loop.
 */
class App {
public:
  /**
   * @brief Callback type for face detection results.
   * @note Uses std::move_only_function when available (C++23), falls back to std::function.
   */
#if CLIENT_HAS_MOVE_ONLY_FUNCTION
  using FaceDetectionCallback = std::move_only_function<void(const FaceDetectionResult&)>;
#else
  using FaceDetectionCallback = std::function<void(const FaceDetectionResult&)>;
#endif

  /**
   * @brief Constructs the application with command line arguments.
   * @param argc Argument count
   * @param argv Argument values
   */
  App(int argc, char** argv) : App(argc, argv, ParseArguments(argc, argv)) {}

  /**
   * @brief Constructs the application with explicit configuration.
   * @param argc Argument count
   * @param argv Argument values
   * @param config Application configuration
   * @param use_gui Whether to use GUI mode (default: auto-detect based on config.headless)
   */
  App(int argc, char** argv, AppConfig config, bool use_gui = true);

  App(const App&) = delete;
  App(App&&) = delete;
  ~App();

  App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;

  /**
   * @brief Runs the main application loop.
   * @return Application return code
   */
  [[nodiscard]] AppReturnCode Run();

  /**
   * @brief Switches to a different face detection model.
   * @warning Application must be running.
   * @param model_type The model type to switch to
   * @return Expected void on success, or AppReturnCode on failure
   */
  [[nodiscard]] auto SwitchModel(ModelType model_type) -> std::expected<void, AppReturnCode>;

  /**
   * @brief Switches to a different camera device.
   * @warning Application must be running.
   * @param device_id The camera device ID to switch to
   * @return Expected void on success, or AppReturnCode on failure
   */
  [[nodiscard]] auto SwitchCamera(std::string_view device_id) -> std::expected<void, AppReturnCode>;

  /**
   * @brief Requests the application to stop gracefully.
   */
  void RequestStop() noexcept { stop_requested_ = true; }

  /**
   * @brief Sets the face detection callback.
   * @param callback The callback to invoke on each detection
   */
  void SetFaceDetectionCallback(FaceDetectionCallback callback) noexcept { detection_callback_ = std::move(callback); }

  /**
   * @brief Checks if the application is running.
   * @return True if running
   */
  [[nodiscard]] bool Running() const noexcept { return running_.load(std::memory_order_acquire); }

  /**
   * @brief Checks if the application is running in GUI mode.
   * @return True if GUI is enabled
   */
  [[nodiscard]] bool GuiEnabled() const noexcept { return use_gui_; }

  /**
   * @brief Gets the application configuration.
   * @return Reference to the current configuration
   */
  [[nodiscard]] const AppConfig& Config() const noexcept { return config_; }

  /**
   * @brief Gets the last face detection result.
   * @return Last detection result, or nullopt if none
   */
  [[nodiscard]] auto LastDetection() const noexcept -> std::optional<FaceDetectionResult> {
    std::scoped_lock lock(detection_mutex_);
    return last_detection_;
  }

  /**
   * @brief Gets the total number of frames processed.
   * @return Frame count
   */
  [[nodiscard]] uint64_t FramesProcessed() const noexcept { return frames_processed_.load(std::memory_order_relaxed); }

  /**
   * @brief Gets the current model type.
   * @return Current model type
   */
  [[nodiscard]] ModelType CurrentModelType() const noexcept { return config_.model_type; }

  /**
   * @brief Gets a reference to the camera.
   * @return Reference to the camera
   */
  [[nodiscard]] Camera& GetCamera() noexcept { return camera_; }

  /**
   * @brief Gets a const reference to the camera.
   * @return Const reference to the camera
   */
  [[nodiscard]] const Camera& GetCamera() const noexcept { return camera_; }

  /**
   * @brief Gets the GUI window.
   * @return Pointer to GUI window, or nullptr if not in GUI mode
   */
  [[nodiscard]] GuiWindow* GetGuiWindow() noexcept { return gui_window_.get(); }

  /**
   * @brief Gets the GUI window.
   * @return Pointer to GUI window, or nullptr if not in GUI mode
   */
  [[nodiscard]] const GuiWindow* GetGuiWindow() const noexcept { return gui_window_.get(); }

  /**
   * @brief Gets the application name.
   * @return Application name string
   */
  [[nodiscard]] static constexpr std::string_view Name() noexcept { return "FaceTracker Client"; }

  /**
   * @brief Gets the application version.
   * @return Version string
   */
  [[nodiscard]] static constexpr std::string_view Version() noexcept { return "0.1.0"; }

private:
  /**
   * @brief Initializes all application components.
   * @return Expected void on success, or AppReturnCode on failure
   */
  [[nodiscard]] auto Initialize() -> std::expected<void, AppReturnCode>;

  /**
   * @brief Processes a single frame from the camera.
   * @param frame The frame to process
   */
  void ProcessFrame(const Frame& frame);

  /**
   * @brief Handles face detection results.
   * @param result The detection result
   * @param frame The frame that was processed
   */
  void HandleDetection(const FaceDetectionResult& result, const Frame& frame);

  /**
   * @brief Updates the GUI with current state.
   */
  void UpdateGui();

  AppConfig config_;

  std::unique_ptr<QCoreApplication> qt_app_;
  std::unique_ptr<GuiWindow> gui_window_;
  Camera camera_;
  comm::BluetoothManager bluetooth_;

  FaceTracker face_tracker_;
  FaceDetectionCallback detection_callback_;

  mutable std::mutex detection_mutex_;
  std::optional<FaceDetectionResult> last_detection_;

  std::atomic<uint64_t> frames_processed_{0};
  std::atomic<bool> running_{false};
  std::atomic<bool> stop_requested_{false};
  bool use_gui_ = false;

  // GUI state (protected by gui_mutex_)
  mutable std::mutex gui_mutex_;
  std::chrono::steady_clock::time_point last_fps_update_;
  uint64_t fps_frame_count_ = 0;
  float current_fps_ = 0.0F;
};

}  // namespace client
