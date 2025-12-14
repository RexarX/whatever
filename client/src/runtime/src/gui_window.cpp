#include <client/app/gui_window.hpp>

#include <client/app/camera.hpp>
#include <client/app/model_config.hpp>
#include <client/app/settings_manager.hpp>

#include <client/core/assert.hpp>
#include <client/core/logger.hpp>

#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QString>
#include <QUrl>
#include <QVariant>

#include <opencv2/imgproc.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string_view>

namespace client {

namespace {}  // namespace

void GuiBackend::UpdateStats(float fps, uint64_t frames_processed, size_t faces_detected) {
  bool changed = false;

  const qreal new_fps = static_cast<qreal>(fps);
  const qreal old_fps = fps_.exchange(new_fps, std::memory_order_relaxed);
  if (old_fps != new_fps) {
    changed = true;
  }

  const quint64 old_frames = frames_processed_.exchange(frames_processed, std::memory_order_relaxed);
  if (old_frames != frames_processed) {
    changed = true;
  }

  const int new_faces = static_cast<int>(faces_detected);
  const int old_faces = faces_detected_.exchange(new_faces, std::memory_order_relaxed);
  if (old_faces != new_faces) {
    changed = true;
  }

  if (changed) {
    emit statsChanged();
  }
}

void GuiBackend::UpdateFaces(const FaceDetectionResult& result) {
  QVariantList face_list;
  face_list.reserve(static_cast<qsizetype>(result.faces.size()));

  for (const auto& face : result.faces) {
    QVariantMap face_data;
    face_data["x"] = static_cast<qreal>(face.bounding_box.x);
    face_data["y"] = static_cast<qreal>(face.bounding_box.y);
    face_data["width"] = static_cast<qreal>(face.bounding_box.width);
    face_data["height"] = static_cast<qreal>(face.bounding_box.height);
    face_data["confidence"] = static_cast<qreal>(face.confidence);
    face_data["distance"] = static_cast<qreal>(face.relative_distance);
    face_data["trackId"] = static_cast<int>(face.track_id);
    face_list.append(face_data);
  }

  {
    std::unique_lock lock(data_mutex_);
    faces_ = std::move(face_list);
  }
  emit facesChanged();
}

void GuiBackend::UpdateCameraList(std::span<const CameraDeviceInfo> cameras, std::string_view current_id) {
  QVariantList new_camera_list;
  new_camera_list.reserve(static_cast<qsizetype>(cameras.size()));

  QString new_current_camera;
  for (const auto& cam : cameras) {
    QVariantMap camera_data;
    camera_data["id"] = QString::fromStdString(cam.id);
    camera_data["description"] = QString::fromStdString(cam.description);
    camera_data["isDefault"] = cam.is_default;
    new_camera_list.append(camera_data);

    if (cam.id == current_id) {
      new_current_camera = QString::fromStdString(cam.id);
    }
  }

  {
    std::unique_lock lock(data_mutex_);
    camera_list_ = std::move(new_camera_list);
    current_camera_ = std::move(new_current_camera);
  }

  emit cameraListChanged();
  emit cameraChanged();

  CLIENT_INFO("Camera list updated in backend: {} cameras available", cameras.size());
}

void GuiBackend::UpdateAvailableDevices(std::span<const BluetoothDeviceInfo> devices) {
  QVariantList device_list;
  device_list.reserve(static_cast<qsizetype>(devices.size()));

  for (const auto& device : devices) {
    QVariantMap device_data;
    device_data["name"] = QString::fromStdString(device.name);
    device_data["address"] = QString::fromStdString(device.address);
    device_list.append(device_data);
  }

  {
    std::unique_lock lock(data_mutex_);
    available_devices_ = std::move(device_list);
  }

  emit availableDevicesChanged();
  CLIENT_INFO("Available devices updated: {} devices found", devices.size());
}

void GuiBackend::SetCurrentModel(ModelType model_type) {
  const int new_type = static_cast<int>(model_type);
  const int old_type = current_model_type_.exchange(new_type, std::memory_order_relaxed);
  if (old_type != new_type) {
    emit modelChanged();
  }
}

void GuiBackend::SetConnectionState(client::ConnectionState state, std::string_view error_message) {
  const auto old_state = connection_state_.exchange(state, std::memory_order_relaxed);

  if (state == client::ConnectionState::kError && !error_message.empty()) {
    std::unique_lock lock(data_mutex_);
    connection_error_message_ = QString::fromUtf8(error_message.data(), static_cast<qsizetype>(error_message.size()));
  } else if (state != client::ConnectionState::kError) {
    std::unique_lock lock(data_mutex_);
    connection_error_message_.clear();
  }

  if (old_state != state) {
    CLIENT_INFO("Connection state changed: {} -> {}", static_cast<int>(old_state), static_cast<int>(state));
    emit connectionStateChanged();
  }
}

QVariantList GuiBackend::getCameraList() const {
  std::shared_lock lock(data_mutex_);
  return camera_list_;
}

void GuiBackend::selectCamera(const QString& deviceId) {
  CLIENT_INFO("Camera selected from QML: {}", deviceId.toStdString());

  if (camera_switch_callback_) {
    camera_switch_callback_(deviceId.toStdString());
  }

  {
    std::unique_lock lock(data_mutex_);
    current_camera_ = deviceId;
  }
  emit cameraChanged();
}

void GuiBackend::selectModel(int modelType) {
  CLIENT_INFO("Model selected from QML: {}", modelType);

  if (model_switch_callback_) {
    model_switch_callback_(static_cast<ModelType>(modelType));
  }

  current_model_type_.store(modelType, std::memory_order_relaxed);
  emit modelChanged();
}

void GuiBackend::applySettings(const QVariantMap& settings) {
  CLIENT_INFO("Settings changed from QML");

  if (settings_changed_callback_) {
    settings_changed_callback_(settings);
  }
}

void GuiBackend::connectToDevice(const QString& deviceAddress) {
  CLIENT_INFO("Connect to device requested from QML: {}", deviceAddress.toStdString());

  if (connect_callback_) {
    connect_callback_(deviceAddress.toStdString());
  }
}

void GuiBackend::disconnectFromDevice() {
  CLIENT_INFO("Disconnect from device requested from QML");

  if (disconnect_callback_) {
    disconnect_callback_();
  }
}

void GuiBackend::scanForDevices() {
  CLIENT_INFO("Scan for devices requested from QML");

  if (scan_callback_) {
    scan_callback_();
  }
}

void GuiBackend::calibrateDevice() {
  CLIENT_INFO("Calibrate device requested from QML");

  if (calibrate_callback_) {
    calibrate_callback_();
  }
}

GuiWindow::GuiWindow(QObject* parent) : QObject(parent) {
  CLIENT_INFO("QML GUI window created");
}

GuiWindow::~GuiWindow() {
  if (window_) {
    window_->close();
  }
  CLIENT_INFO("QML GUI window destroyed");
}

bool GuiWindow::Initialize() {
  bool expected = false;
  if (!initialized_.compare_exchange_strong(expected, true, std::memory_order_acquire)) {
    return qml_loaded_.load(std::memory_order_acquire);
  }

  const bool result = InitializeQml();
  qml_loaded_.store(result, std::memory_order_release);
  return result;
}

bool GuiWindow::InitializeQml() {
  // Verify QApplication exists before trying to load QML
  if (!QCoreApplication::instance()) {
    CLIENT_ERROR("QCoreApplication not initialized - cannot load QML");
    return false;
  }

  CLIENT_INFO("Qt version: {}.{}.{}", QT_VERSION_MAJOR, QT_VERSION_MINOR, QT_VERSION_PATCH);
  CLIENT_INFO("QCoreApplication instance valid: {}", QCoreApplication::instance() != nullptr);

  // Create backend
  backend_ = std::make_unique<GuiBackend>(this);

  // Create settings manager
  settings_manager_ = std::make_unique<SettingsManager>(this);

  // Create image provider (will be owned by the engine)
  image_provider_ = new FrameImageProvider();

  // Create QML engine
  engine_ = std::make_unique<QQmlApplicationEngine>();

  // Add image provider to engine
  engine_->addImageProvider("frames", image_provider_);

  // Expose backend and settings to QML
  engine_->rootContext()->setContextProperty("backend", backend_.get());
  engine_->rootContext()->setContextProperty("settings", settings_manager_.get());

  // Connect backend quit signal
  connect(backend_.get(), &GuiBackend::quitRequested, this, &GuiWindow::QuitRequested);

  // Connect frame update signal for thread-safe updates
  connect(this, &GuiWindow::frameUpdated, this, [this]() {
    if (window_) {
      const uint64_t counter = frame_counter_.load(std::memory_order_relaxed);
      QMetaObject::invokeMethod(window_, "updateFrame",
                                Q_ARG(QVariant, QString("image://frames/frame?%1").arg(counter)));
    }
  });

  // QML resource paths vary based on how the QML module is configured:
  // - qt_add_qml_module with RESOURCE_PREFIX /qt/qml: qrc:/qt/qml/client/Main.qml
  // - qt_add_qml_module with RESOURCE_PREFIX /: qrc:/client/Main.qml
  // - Legacy .qrc file: qrc:/Main.qml
  //
  // We try multiple paths to find the QML file, prioritizing qt_add_qml_module paths.
  const std::array<QString, 6> qml_paths = {
      QStringLiteral("qrc:/qt/qml/Main.qml"),         // qt_add_qml_module with NO_RESOURCE_TARGET_PATH
      QStringLiteral("qrc:/qt/qml/client/Main.qml"),  // qt_add_qml_module with RESOURCE_PREFIX /qt/qml
      QStringLiteral("qrc:/client/Main.qml"),         // qt_add_qml_module with RESOURCE_PREFIX /
      QStringLiteral("qrc:/Main.qml"),                // Legacy .qrc file
      QStringLiteral("qrc:/qml/Main.qml"),            // Another alternative
      QStringLiteral("qrc:/client/qt/qml/Main.qml"),  // Alternative with client prefix
  };

  // Try to load QML files in order of preference
  // QFile::exists() doesn't work reliably with QRC files, so we try loading instead
  bool load_failed = false;
  QUrl successful_url;

  QObject::connect(
      engine_.get(), &QQmlApplicationEngine::objectCreated, this,
      [this, &load_failed, &successful_url](QObject* obj, const QUrl& objUrl) {
        if (!obj && objUrl == successful_url) {
          load_failed = true;
          return;
        }

        if (obj && !successful_url.isEmpty()) {
          // Get the window reference
          window_ = qobject_cast<QQuickWindow*>(obj);
          if (window_) {
            // Connect window closing to quit signal
            connect(window_, &QQuickWindow::closing, this, [this]() {
              CLIENT_INFO("QML window closing");
              emit QuitRequested();
            });
          }
        }
      },
      Qt::DirectConnection);

  // Try each path until one successfully loads
  for (const auto& path : qml_paths) {
    successful_url = QUrl(path);
    load_failed = false;

    CLIENT_INFO("Attempting to load QML file: {}", path.toStdString());
    engine_->load(successful_url);

    // Check if loading succeeded
    if (!load_failed && !engine_->rootObjects().isEmpty()) {
      CLIENT_INFO("Successfully loaded QML file from: {}", path.toStdString());
      break;
    }

    // Clear for next attempt
    engine_->clearComponentCache();
    successful_url.clear();
  }

  // If all paths failed, log error
  if (successful_url.isEmpty() || load_failed || engine_->rootObjects().isEmpty()) {
    CLIENT_WARN("QML file not found at any expected path. Attempted paths:");
    for (const auto& path : qml_paths) {
      CLIENT_WARN("  - {}", path.toStdString());
    }
  }

  // Wait for QML to load
  QCoreApplication::processEvents();

  // Check if loading succeeded
  if (load_failed || engine_->rootObjects().isEmpty()) {
    CLIENT_ERROR("QML GUI loading failed - root objects: {}, load_failed: {}", engine_->rootObjects().isEmpty(),
                 load_failed);
    CLIENT_WARN("Application will run in headless mode");
    CLIENT_INFO("Application will function without visual interface but all face detection works");
    return false;
  }

  CLIENT_INFO("QML engine initialized successfully with {} root objects", engine_->rootObjects().size());
  return true;
}

void GuiWindow::show() {
  if (!initialized_.load(std::memory_order_acquire)) {
    CLIENT_WARN("GuiWindow::show() called before Initialize()");
    return;
  }

  if (window_ && qml_loaded_.load(std::memory_order_acquire)) {
    window_->show();
    CLIENT_INFO("QML window shown");
  }
}

void GuiWindow::close() {
  if (window_ && qml_loaded_.load(std::memory_order_acquire)) {
    window_->close();
    CLIENT_INFO("QML window closed");
  }
}

bool GuiWindow::isVisible() const {
  return qml_loaded_.load(std::memory_order_acquire) && window_ && window_->isVisible();
}

QImage GuiWindow::FrameToQImage(const Frame& frame) {
  if (frame.Empty()) {
    return {};
  }

  // Convert BGR to RGB
  cv::Mat rgb_frame;
  cv::cvtColor(frame.Mat(), rgb_frame, cv::COLOR_BGR2RGB);

  // Create QImage from cv::Mat data
  QImage image(rgb_frame.data, rgb_frame.cols, rgb_frame.rows, static_cast<qsizetype>(rgb_frame.step),
               QImage::Format_RGB888);

  // Return a copy since rgb_frame will be destroyed
  return image.copy();
}

void GuiWindow::UpdateFrame(const Frame& frame, const std::optional<FaceDetectionResult>& result) {
  if (frame.Empty()) {
    return;
  }

  last_frame_ = frame.Clone();

  // Convert to QImage and update provider
  QImage image = FrameToQImage(frame);
  if (!image.isNull() && image_provider_) {
    image_provider_->UpdateImage(std::move(image));

    frame_counter_.fetch_add(1, std::memory_order_relaxed);

    // Emit signal to update QML (thread-safe) - only if QML is loaded
    if (qml_loaded_.load(std::memory_order_acquire)) {
      emit frameUpdated();
    }
  }

  // Update face data in backend
  if (result && backend_) {
    backend_->UpdateFaces(*result);
  }
}

void GuiWindow::UpdateCameraList(std::span<const CameraDeviceInfo> cameras, std::string_view current_id) {
  if (backend_) {
    backend_->UpdateCameraList(cameras, current_id);

    // Also update QML directly - only if QML is loaded
    if (qml_loaded_.load(std::memory_order_acquire) && window_) {
      QVariantList camera_list;
      QString current_camera_id = QString::fromUtf8(current_id.data(), static_cast<qsizetype>(current_id.size()));

      for (const auto& cam : cameras) {
        QVariantMap camera_data;
        camera_data["id"] = QString::fromStdString(cam.id);
        camera_data["description"] = QString::fromStdString(cam.description);
        camera_data["isDefault"] = cam.is_default;
        camera_list.append(camera_data);
      }

      QMetaObject::invokeMethod(window_, "updateCameraList", Q_ARG(QVariant, QVariant::fromValue(camera_list)),
                                Q_ARG(QVariant, current_camera_id));
    }
  }

  CLIENT_INFO("Camera list updated: {} cameras available", cameras.size());
}

void GuiWindow::UpdateStats(float fps, uint64_t frames_processed, size_t faces_detected) {
  if (backend_) {
    backend_->UpdateStats(fps, frames_processed, faces_detected);
  }
}

void GuiWindow::SetCurrentModel(ModelType model_type) {
  if (backend_) {
    backend_->SetCurrentModel(model_type);

    // Also update QML directly - only if QML is loaded
    if (qml_loaded_.load(std::memory_order_acquire) && window_) {
      QMetaObject::invokeMethod(window_, "setCurrentModel", Q_ARG(QVariant, static_cast<int>(model_type)));
    }
  }
}

void GuiWindow::SetConnectionState(ConnectionState state, std::string_view error_message) {
  if (backend_) {
    backend_->SetConnectionState(state, error_message);
  }
}

void GuiWindow::UpdateAvailableDevices(std::span<const BluetoothDeviceInfo> devices) {
  if (backend_) {
    backend_->UpdateAvailableDevices(devices);

    // Also update QML directly - only if QML is loaded
    if (qml_loaded_.load(std::memory_order_acquire) && window_) {
      QVariantList device_list;

      for (const auto& device : devices) {
        QVariantMap device_data;
        device_data["name"] = QString::fromStdString(device.name);
        device_data["address"] = QString::fromStdString(device.address);
        device_list.append(device_data);
      }

      QMetaObject::invokeMethod(window_, "updateAvailableDevices", Q_ARG(QVariant, QVariant::fromValue(device_list)));
    }
  }
}

void GuiWindow::SetCameraSwitchCallback(CameraSwitchCallback callback) noexcept {
  if (backend_) {
    backend_->SetCameraSwitchCallback(std::move(callback));
  }
}

void GuiWindow::SetModelSwitchCallback(ModelSwitchCallback callback) noexcept {
  if (backend_) {
    backend_->SetModelSwitchCallback(std::move(callback));
  }
}

void GuiWindow::SetSettingsChangedCallback(SettingsChangedCallback callback) noexcept {
  if (backend_) {
    backend_->SetSettingsChangedCallback(std::move(callback));
  }
}

void GuiWindow::SetConnectCallback(ConnectCallback callback) noexcept {
  if (backend_) {
    backend_->SetConnectCallback(std::move(callback));
  }
}

void GuiWindow::SetDisconnectCallback(DisconnectCallback callback) noexcept {
  if (backend_) {
    backend_->SetDisconnectCallback(std::move(callback));
  }
}

void GuiWindow::SetScanCallback(ScanCallback callback) noexcept {
  if (backend_) {
    backend_->SetScanCallback(std::move(callback));
  }
}

void GuiWindow::SetCalibrateCallback(CalibrateCallback callback) noexcept {
  if (backend_) {
    backend_->SetCalibrateCallback(std::move(callback));
  }
}

}  // namespace client
