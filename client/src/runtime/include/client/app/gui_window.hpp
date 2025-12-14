#pragma once

#include <client/pch.hpp>

#include <client/app/face_data.hpp>
#include <client/app/frame.hpp>
#include <client/core/logger.hpp>

#include <QImage>
#include <QObject>
#include <QQuickImageProvider>
#include <QQuickWindow>
#include <QString>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>

class QQmlApplicationEngine;

namespace client {

// Forward declarations
struct CameraDeviceInfo;
enum class ModelType : uint8_t;
class SettingsManager;

/**
 * @brief Bluetooth connection state enumeration.
 */
enum class ConnectionState : int { kDisconnected = 0, kConnecting = 1, kConnected = 2, kError = 3 };

/**
 * @brief Information about a discovered Bluetooth device.
 */
struct BluetoothDeviceInfo final {
  std::string name;     ///< Device name.
  std::string address;  ///< Device address (MAC address or UUID).

  [[nodiscard]] bool operator==(const BluetoothDeviceInfo&) const noexcept = default;
};

/**
 * @brief Image provider for QML to display camera frames.
 * @details Provides a thread-safe way to update video frames from the camera
 * and display them in QML.
 */
class FrameImageProvider final : public QQuickImageProvider {
public:
  FrameImageProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
  FrameImageProvider(const FrameImageProvider&) = delete;
  FrameImageProvider(FrameImageProvider&&) = delete;
  ~FrameImageProvider() override = default;

  FrameImageProvider& operator=(const FrameImageProvider&) = delete;
  FrameImageProvider& operator=(FrameImageProvider&&) = delete;

  /**
   * @brief Provides image to QML.
   * @param id Image ID (unused, we only have one image)
   * @param size Output size pointer
   * @param requestedSize Requested image size
   * @return The current frame as QImage
   */
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize) override;

  /**
   * @brief Updates the current frame.
   * @param image The new frame image
   */
  void UpdateImage(QImage image);

private:
  mutable std::shared_mutex mutex_;
  QImage current_image_;
};

inline QImage FrameImageProvider::requestImage(const QString& /*id*/, QSize* size, const QSize& requestedSize) {
  std::shared_lock lock(mutex_);

  if (current_image_.isNull()) {
    // Return a placeholder
    QImage placeholder(320, 240, QImage::Format_RGB888);
    placeholder.fill(Qt::black);
    if (size) {
      *size = placeholder.size();
    }
    return placeholder;
  }

  QImage result = current_image_;

  if (requestedSize.isValid() && requestedSize != result.size()) {
    result = result.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
  }

  if (size) {
    *size = result.size();
  }

  return result;
}

inline void FrameImageProvider::UpdateImage(QImage image) {
  std::unique_lock lock(mutex_);
  current_image_ = std::move(image);
}

/**
 * @brief QML backend bridge for exposing C++ data to QML.
 * @details This object is registered with QML and provides properties
 * and methods that the QML UI can access.
 */
class GuiBackend final : public QObject {
  Q_OBJECT

  Q_PROPERTY(qreal fps READ Fps NOTIFY statsChanged)
  Q_PROPERTY(quint64 framesProcessed READ FramesProcessed NOTIFY statsChanged)
  Q_PROPERTY(int facesDetected READ FacesDetected NOTIFY statsChanged)
  Q_PROPERTY(QString currentCamera READ CurrentCamera NOTIFY cameraChanged)
  Q_PROPERTY(int currentModelType READ CurrentModelType NOTIFY modelChanged)
  Q_PROPERTY(QVariantList faces READ Faces NOTIFY facesChanged)
  Q_PROPERTY(int connectionState READ ConnectionStateValue NOTIFY connectionStateChanged)
  Q_PROPERTY(QString connectionErrorMessage READ ConnectionErrorMessage NOTIFY connectionStateChanged)
  Q_PROPERTY(QVariantList availableDevices READ AvailableDevices NOTIFY availableDevicesChanged)

public:
  /**
   * @brief Callback type for camera switch requests.
   */
#if defined(__cpp_lib_move_only_function)
  using CameraSwitchCallback = std::move_only_function<void(std::string_view device_id)>;
#else
  using CameraSwitchCallback = std::function<void(std::string_view device_id)>;
#endif

  /**
   * @brief Callback type for model switch requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ModelSwitchCallback = std::move_only_function<void(ModelType model_type)>;
#else
  using ModelSwitchCallback = std::function<void(ModelType model_type)>;
#endif

  /**
   * @brief Callback type for settings changes.
   */
#if defined(__cpp_lib_move_only_function)
  using SettingsChangedCallback = std::move_only_function<void(const QVariantMap& settings)>;
#else
  using SettingsChangedCallback = std::function<void(const QVariantMap& settings)>;
#endif

  /**
   * @brief Callback type for Bluetooth connection requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ConnectCallback = std::move_only_function<void(std::string_view device_address)>;
#else
  using ConnectCallback = std::function<void(std::string_view device_address)>;
#endif

  /**
   * @brief Callback type for Bluetooth disconnect requests.
   */
#if defined(__cpp_lib_move_only_function)
  using DisconnectCallback = std::move_only_function<void()>;
#else
  using DisconnectCallback = std::function<void()>;
#endif

  /**
   * @brief Callback type for Bluetooth scan requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ScanCallback = std::move_only_function<void()>;
#else
  using ScanCallback = std::function<void()>;
#endif

  /**
   * @brief Callback type for calibration requests.
   */
#if defined(__cpp_lib_move_only_function)
  using CalibrateCallback = std::move_only_function<void()>;
#else
  using CalibrateCallback = std::function<void()>;
#endif

  explicit GuiBackend(QObject* parent = nullptr) : QObject(parent) { CLIENT_INFO("GuiBackend created"); }
  GuiBackend(const GuiBackend&) = delete;
  GuiBackend(GuiBackend&&) = delete;
  ~GuiBackend() override = default;

  GuiBackend& operator=(const GuiBackend&) = delete;
  GuiBackend& operator=(GuiBackend&&) = delete;

  /**
   * @brief Updates statistics displayed in QML.
   * @param fps Current FPS
   * @param frames_processed Total frames processed
   * @param faces_detected Number of faces in current frame
   */
  void UpdateStats(float fps, uint64_t frames_processed, size_t faces_detected);

  /**
   * @brief Updates the face detection data.
   * @param result Face detection result with face data
   */
  void UpdateFaces(const FaceDetectionResult& result);

  /**
   * @brief Updates the camera list in the UI.
   * @param cameras List of available cameras.
   * @param current_id ID of the currently active camera
   */
  void UpdateCameraList(std::span<const CameraDeviceInfo> cameras, std::string_view current_id);

  /**
   * @brief Sets the current model type in the UI.
   * @param model_type The currently active model type
   */
  void SetCurrentModel(ModelType model_type);

  /**
   * @brief Sets the connection state.
   * @param state New connection state
   * @param error_message Optional error message (only used when state is kError)
   */
  void SetConnectionState(client::ConnectionState state, std::string_view error_message = "");

  /**
   * @brief Updates the list of available Bluetooth devices.
   * @param devices List of discovered devices
   */
  void UpdateAvailableDevices(std::span<const BluetoothDeviceInfo> devices);

  /**
   * @brief Sets the camera switch callback.
   * @param callback Callback to invoke when user requests camera switch
   */
  void SetCameraSwitchCallback(CameraSwitchCallback callback) noexcept {
    camera_switch_callback_ = std::move(callback);
  }

  /**
   * @brief Sets the model switch callback.
   * @param callback Callback to invoke when user requests model switch
   */
  void SetModelSwitchCallback(ModelSwitchCallback callback) noexcept { model_switch_callback_ = std::move(callback); }

  /**
   * @brief Sets the settings changed callback.
   * @param callback Callback to invoke when settings change
   */
  void SetSettingsChangedCallback(SettingsChangedCallback callback) noexcept {
    settings_changed_callback_ = std::move(callback);
  }

  /**
   * @brief Sets the connect callback.
   * @param callback Callback to invoke when user requests Bluetooth connection
   */
  void SetConnectCallback(ConnectCallback callback) noexcept { connect_callback_ = std::move(callback); }

  /**
   * @brief Sets the disconnect callback.
   * @param callback Callback to invoke when user requests Bluetooth disconnection
   */
  void SetDisconnectCallback(DisconnectCallback callback) noexcept { disconnect_callback_ = std::move(callback); }

  /**
   * @brief Sets the scan callback.
   * @param callback Callback to invoke when user requests Bluetooth device scan
   */
  void SetScanCallback(ScanCallback callback) noexcept { scan_callback_ = std::move(callback); }

  /**
   * @brief Sets the calibrate callback.
   * @param callback Callback to invoke when user requests calibration
   */
  void SetCalibrateCallback(CalibrateCallback callback) noexcept { calibrate_callback_ = std::move(callback); }

  // Property getters
  [[nodiscard]] qreal Fps() const noexcept { return fps_.load(std::memory_order_relaxed); }

  [[nodiscard]] quint64 FramesProcessed() const noexcept { return frames_processed_.load(std::memory_order_relaxed); }

  [[nodiscard]] int FacesDetected() const noexcept { return faces_detected_.load(std::memory_order_relaxed); }

  [[nodiscard]] QString CurrentCamera() const noexcept {
    std::shared_lock lock(data_mutex_);
    return current_camera_;
  }

  [[nodiscard]] int CurrentModelType() const noexcept { return current_model_type_.load(std::memory_order_relaxed); }

  [[nodiscard]] QVariantList Faces() const noexcept {
    std::shared_lock lock(data_mutex_);
    return faces_;
  }

  [[nodiscard]] int ConnectionStateValue() const noexcept {
    return static_cast<int>(connection_state_.load(std::memory_order_relaxed));
  }

  [[nodiscard]] client::ConnectionState GetConnectionState() const noexcept {
    return connection_state_.load(std::memory_order_relaxed);
  }

  [[nodiscard]] QString ConnectionErrorMessage() const noexcept {
    std::shared_lock lock(data_mutex_);
    return connection_error_message_;
  }

  [[nodiscard]] QVariantList AvailableDevices() const noexcept {
    std::shared_lock lock(data_mutex_);
    return available_devices_;
  }

  /**
   * @brief Gets the camera list as QVariantList for QML.
   * @return List of camera info objects
   */
  Q_INVOKABLE QVariantList getCameraList() const;

public slots:
  /**
   * @brief Called from QML when a camera is selected.
   * @param deviceId The selected camera device ID
   */
  void selectCamera(const QString& deviceId);

  /**
   * @brief Called from QML when a model is selected.
   * @param modelType The selected model type index
   */
  void selectModel(int modelType);

  /**
   * @brief Called from QML when settings change.
   * @param settings Map of changed settings
   */
  void applySettings(const QVariantMap& settings);

  /**
   * @brief Called from QML when user wants to connect to a Bluetooth device.
   * @param deviceAddress The address of the device to connect to
   */
  void connectToDevice(const QString& deviceAddress);

  /**
   * @brief Called from QML when user wants to disconnect from the current device.
   */
  void disconnectFromDevice();

  /**
   * @brief Called from QML when user wants to scan for Bluetooth devices.
   */
  void scanForDevices();

  /**
   * @brief Called from QML when user wants to calibrate the device.
   */
  void calibrateDevice();

signals:
  void statsChanged();
  void facesChanged();
  void cameraChanged();
  void modelChanged();
  void cameraListChanged();
  void connectionStateChanged();
  void availableDevicesChanged();
  void quitRequested();

private:
  std::atomic<qreal> fps_{0.0};
  std::atomic<quint64> frames_processed_{0};
  std::atomic<int> faces_detected_{0};
  std::atomic<int> current_model_type_{0};
  std::atomic<client::ConnectionState> connection_state_{client::ConnectionState::kDisconnected};

  mutable std::shared_mutex data_mutex_;
  QString current_camera_;
  QVariantList faces_;
  QVariantList camera_list_;
  QVariantList available_devices_;
  QString connection_error_message_;

  CameraSwitchCallback camera_switch_callback_;
  ModelSwitchCallback model_switch_callback_;
  SettingsChangedCallback settings_changed_callback_;
  ConnectCallback connect_callback_;
  DisconnectCallback disconnect_callback_;
  ScanCallback scan_callback_;
  CalibrateCallback calibrate_callback_;
};

/**
 * @brief Modern QML-based GUI window for face tracker visualization.
 * @details Provides a cross-platform QML GUI with camera preview, face detection overlay,
 * animated controls, and a settings panel. Uses Material design styling.
 */
class GuiWindow final : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Callback type for camera switch requests.
   */
#if defined(__cpp_lib_move_only_function)
  using CameraSwitchCallback = std::move_only_function<void(std::string_view device_id)>;
#else
  using CameraSwitchCallback = std::function<void(std::string_view device_id)>;
#endif

  /**
   * @brief Callback type for model switch requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ModelSwitchCallback = std::move_only_function<void(ModelType model_type)>;
#else
  using ModelSwitchCallback = std::function<void(ModelType model_type)>;
#endif

  /**
   * @brief Callback type for settings changes.
   */
#if defined(__cpp_lib_move_only_function)
  using SettingsChangedCallback = std::move_only_function<void(const QVariantMap& settings)>;
#else
  using SettingsChangedCallback = std::function<void(const QVariantMap& settings)>;
#endif

  /**
   * @brief Callback type for Bluetooth connection requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ConnectCallback = std::move_only_function<void(std::string_view device_address)>;
#else
  using ConnectCallback = std::function<void(std::string_view device_address)>;
#endif

  /**
   * @brief Callback type for Bluetooth disconnect requests.
   */
#if defined(__cpp_lib_move_only_function)
  using DisconnectCallback = std::move_only_function<void()>;
#else
  using DisconnectCallback = std::function<void()>;
#endif

  /**
   * @brief Callback type for Bluetooth scan requests.
   */
#if defined(__cpp_lib_move_only_function)
  using ScanCallback = std::move_only_function<void()>;
#else
  using ScanCallback = std::function<void()>;
#endif

  /**
   * @brief Callback type for calibration requests.
   */
#if defined(__cpp_lib_move_only_function)
  using CalibrateCallback = std::move_only_function<void()>;
#else
  using CalibrateCallback = std::function<void()>;
#endif

  /**
   * @brief Constructs the GUI window.
   * @param parent Optional parent object.
   */
  explicit GuiWindow(QObject* parent = nullptr);

  GuiWindow(const GuiWindow&) = delete;
  GuiWindow(GuiWindow&&) = delete;
  ~GuiWindow() override;

  GuiWindow& operator=(const GuiWindow&) = delete;
  GuiWindow& operator=(GuiWindow&&) = delete;

  /**
   * @brief Initializes the QML engine and loads the UI.
   * @details Must be called after QApplication is created and before show().
   * @return True if initialization succeeded, false if QML loading failed (will fall back to headless mode)
   */
  bool Initialize();

  /**
   * @brief Shows the window.
   */
  void show();

  /**
   * @brief Closes the window.
   */
  void close();

  /**
   * @brief Updates the displayed frame with face detection results.
   * @param frame The camera frame to display
   * @param result Optional face detection result to overlay
   */
  void UpdateFrame(const Frame& frame, const std::optional<FaceDetectionResult>& result = std::nullopt);

  /**
   * @brief Updates the camera list in the UI.
   * @param cameras List of available cameras
   * @param current_id ID of the currently active camera
   */
  void UpdateCameraList(std::span<const CameraDeviceInfo> cameras, std::string_view current_id);

  /**
   * @brief Updates the statistics display.
   * @param fps Current FPS
   * @param frames_processed Total frames processed
   * @param faces_detected Number of faces in current frame
   */
  void UpdateStats(float fps, uint64_t frames_processed, size_t faces_detected);

  /**
   * @brief Updates the list of available Bluetooth devices.
   * @param devices List of discovered devices
   */
  void UpdateAvailableDevices(std::span<const BluetoothDeviceInfo> devices);

  /**
   * @brief Sets the camera switch callback.
   * @param callback Callback to invoke when user requests camera switch
   */
  void SetCameraSwitchCallback(CameraSwitchCallback callback) noexcept;

  /**
   * @brief Sets the model switch callback.
   * @param callback Callback to invoke when user requests model switch
   */
  void SetModelSwitchCallback(ModelSwitchCallback callback) noexcept;

  /**
   * @brief Sets the settings changed callback.
   * @param callback Callback to invoke when settings change
   */
  void SetSettingsChangedCallback(SettingsChangedCallback callback) noexcept;

  /**
   * @brief Sets the current model type in the UI.
   * @param model_type The currently active model type
   */
  void SetCurrentModel(ModelType model_type);

  /**
   * @brief Sets the Bluetooth connection state.
   * @param state New connection state
   * @param error_message Optional error message (only used when state is kError)
   */
  void SetConnectionState(ConnectionState state, std::string_view error_message = "");

  /**
   * @brief Sets the connect callback.
   * @param callback Callback to invoke when user requests Bluetooth connection
   */
  void SetConnectCallback(ConnectCallback callback) noexcept;

  /**
   * @brief Sets the disconnect callback.
   * @param callback Callback to invoke when user requests Bluetooth disconnection
   */
  void SetDisconnectCallback(DisconnectCallback callback) noexcept;

  /**
   * @brief Sets the scan callback.
   * @param callback Callback to invoke when user requests Bluetooth device scan
   */
  void SetScanCallback(ScanCallback callback) noexcept;

  /**
   * @brief Sets the calibrate callback.
   * @param callback Callback to invoke when user requests calibration
   */
  void SetCalibrateCallback(CalibrateCallback callback) noexcept;

  /**
   * @brief Checks if the window is visible.
   * @return True if visible
   */
  [[nodiscard]] bool isVisible() const;

signals:
  /**
   * @brief Signal emitted when the user requests to quit the application.
   */
  void QuitRequested();

  /**
   * @brief Signal to trigger frame update in QML (thread-safe).
   */
  void frameUpdated();

private:
  /**
   * @brief Initializes the QML engine and loads the UI.
   * @return True if initialization succeeded, false otherwise
   */
  bool InitializeQml();

  /**
   * @brief Converts a Frame to QImage.
   * @param frame The frame to convert
   * @return QImage representation
   */
  [[nodiscard]] static QImage FrameToQImage(const Frame& frame);

  std::unique_ptr<QQmlApplicationEngine> engine_;
  std::unique_ptr<GuiBackend> backend_;
  std::unique_ptr<SettingsManager> settings_manager_;
  FrameImageProvider* image_provider_ = nullptr;  // Owned by QML engine
  QQuickWindow* window_ = nullptr;                // Owned by QML engine

  Frame last_frame_;
  std::atomic<uint64_t> frame_counter_{0};
  std::atomic<bool> qml_loaded_{false};
  std::atomic<bool> initialized_{false};
};

}  // namespace client
