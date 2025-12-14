#include <client/app/app.hpp>

#include <client/app/gui_window.hpp>
#include <client/app/model_config.hpp>
#include <client/core/assert.hpp>
#include <client/core/logger.hpp>

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QTimer>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QPermissions>
#endif

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <mutex>
#include <string>
#include <string_view>

namespace client {

/**
 * @brief Make Android embedded model files available via real filesystem paths.
 * @details On Android, model files are packaged into the APK as Qt resources under
 * `:/models/...`. OpenCV cannot load models directly from Qt resources,
 * so the files must be extracted to a writable directory first (e.g. AppDataLocation).
 *
 * On non-Android platforms (or when model paths are already absolute),
 * this function does nothing and returns true.
 *
 * @param config Application config to update in-place.
 * @return True on success or if no action is needed; false on failure.
 */
[[nodiscard]] bool ResolveEmbeddedModelsIfNeeded(AppConfig& config) noexcept;

namespace {

enum class ModelResolveError : uint8_t {
  kAppDataDirNotAvailable,
  kCannotCreateModelsDir,
  kResourceMissing,
  kCannotOpenResource,
  kCannotWriteFile
};

[[nodiscard]] constexpr std::string_view ModelResolveErrorToString(ModelResolveError error) noexcept {
  switch (error) {
    case ModelResolveError::kAppDataDirNotAvailable:
      return "AppDataLocation not available";
    case ModelResolveError::kCannotCreateModelsDir:
      return "Cannot create models directory";
    case ModelResolveError::kResourceMissing:
      return "Embedded model resource missing";
    case ModelResolveError::kCannotOpenResource:
      return "Cannot open embedded model resource";
    case ModelResolveError::kCannotWriteFile:
      return "Cannot write extracted model file";
  }
  return "Unknown error";
}

[[nodiscard]] consteval bool IsAndroid() noexcept {
#if defined(Q_OS_ANDROID)
  return true;
#else
  return false;
#endif
}

[[nodiscard]] auto AndroidModelsExtractDir() -> std::expected<QString, ModelResolveError> {
  const auto app_data_dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  if (app_data_dir.isEmpty()) {
    return std::unexpected(ModelResolveError::kAppDataDirNotAvailable);
  }

  QDir dir(app_data_dir);
  if (!dir.exists() && !dir.mkpath(".")) {
    return std::unexpected(ModelResolveError::kCannotCreateModelsDir);
  }

  const auto models_dir = dir.filePath("models");
  QDir models(models_dir);
  if (!models.exists() && !dir.mkpath("models")) {
    return std::unexpected(ModelResolveError::kCannotCreateModelsDir);
  }

  return models_dir;
}

[[nodiscard]] auto EnsureFileFromResource(QStringView resource_path, const QString& output_path)
    -> std::expected<void, ModelResolveError> {
  CLIENT_ASSERT(!resource_path.isEmpty(), "resource_path must not be empty");
  CLIENT_ASSERT(!output_path.isEmpty(), "output_path must not be empty");

  const auto resource_path_str = resource_path.toString();
  if (!QFileInfo::exists(resource_path_str)) {
    return std::unexpected(ModelResolveError::kResourceMissing);
  }

  // If already extracted, keep it (avoid extra IO on every launch).
  if (QFileInfo::exists(output_path)) {
    return {};
  }

  QFile in(resource_path_str);
  if (!in.open(QIODevice::ReadOnly)) {
    return std::unexpected(ModelResolveError::kCannotOpenResource);
  }

  QFile out(output_path);
  if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return std::unexpected(ModelResolveError::kCannotWriteFile);
  }

  constexpr qint64 kChunkSize = 256 * 1024;
  while (!in.atEnd()) {
    const auto chunk = in.read(kChunkSize);
    if (chunk.isEmpty() && !in.atEnd()) {
      return std::unexpected(ModelResolveError::kCannotOpenResource);
    }
    if (out.write(chunk) != chunk.size()) {
      return std::unexpected(ModelResolveError::kCannotWriteFile);
    }
  }

  out.flush();
  return {};
}

[[nodiscard]] auto ResolveEmbeddedModelsIfNeededImpl(AppConfig& config) -> std::expected<void, ModelResolveError> {
  if (!IsAndroid()) {
    return {};
  }

  // Only resolve when using default relative "models/..." paths.
  // If you passed a custom absolute path via CLI, we don't override it.
  const auto model_is_relative = config.face_tracker.model_path.is_relative();
  const auto config_is_relative =
      config.face_tracker.config_path.empty() ? false : config.face_tracker.config_path.is_relative();

  if (!model_is_relative && !config_is_relative) {
    return {};
  }

  const auto models_dir_result = AndroidModelsExtractDir();
  if (!models_dir_result) {
    return std::unexpected(models_dir_result.error());
  }
  const auto& models_dir = *models_dir_result;

  // Extract the known embedded files (from qt/resources/models.qrc).
  // These are always extracted to the app's private storage.
  const auto yunet_dst = QDir(models_dir).filePath("face_detection_yunet_2023mar.onnx");
  const auto res10_model_dst = QDir(models_dir).filePath("res10_300x300_ssd_iter_140000.caffemodel");
  const auto res10_cfg_dst = QDir(models_dir).filePath("res10_300x300_ssd_deploy.prototxt");
  const auto res10_cfg_broken_dst = QDir(models_dir).filePath("res10_300x300_ssd_deploy_broken.prototxt");

  {
    const auto r = EnsureFileFromResource(QStringLiteral(":/models/face_detection_yunet_2023mar.onnx"), yunet_dst);
    if (!r) {
      return std::unexpected(r.error());
    }
  }
  {
    const auto r =
        EnsureFileFromResource(QStringLiteral(":/models/res10_300x300_ssd_iter_140000.caffemodel"), res10_model_dst);
    if (!r) {
      return std::unexpected(r.error());
    }
  }
  {
    const auto r = EnsureFileFromResource(QStringLiteral(":/models/res10_300x300_ssd_deploy.prototxt"), res10_cfg_dst);
    if (!r) {
      return std::unexpected(r.error());
    }
  }
  {
    const auto r = EnsureFileFromResource(QStringLiteral(":/models/res10_300x300_ssd_deploy_broken.prototxt"),
                                          res10_cfg_broken_dst);
    if (!r) {
      return std::unexpected(r.error());
    }
  }

  // Rewrite config paths to extracted files if they look like the default relative paths.
  // This keeps the rest of the code unchanged (std::filesystem + OpenCV expect real paths).
  const auto model_filename = QString::fromStdString(config.face_tracker.model_path.filename().string());
  const auto config_filename = config.face_tracker.config_path.empty()
                                   ? QString()
                                   : QString::fromStdString(config.face_tracker.config_path.filename().string());

  if (model_is_relative) {
    if (model_filename == "face_detection_yunet_2023mar.onnx") {
      config.face_tracker.model_path = yunet_dst.toStdString();
    } else if (model_filename == "res10_300x300_ssd_iter_140000.caffemodel") {
      config.face_tracker.model_path = res10_model_dst.toStdString();
    }
  }

  if (config_is_relative) {
    if (config_filename == "res10_300x300_ssd_deploy.prototxt") {
      config.face_tracker.config_path = res10_cfg_dst.toStdString();
    } else if (config_filename == "res10_300x300_ssd_deploy_broken.prototxt") {
      config.face_tracker.config_path = res10_cfg_broken_dst.toStdString();
    }
  }

  CLIENT_INFO("Android models resolved to: {}", models_dir.toStdString());
  return {};
}

}  // namespace

[[nodiscard]] bool ResolveEmbeddedModelsIfNeeded(AppConfig& config) noexcept {
  const auto result = ResolveEmbeddedModelsIfNeededImpl(config);
  if (!result) {
    CLIENT_ERROR("Failed to resolve embedded models: {}", ModelResolveErrorToString(result.error()));
    return false;
  }
  return true;
}

/**
 * @brief Resolves model paths for Android for a given ModelConfig
 * @param config The model config to resolve paths for
 * @return Resolved ModelConfig with Android paths
 */
[[nodiscard]] ModelConfig ResolveModelConfigForAndroid(const ModelConfig& config) noexcept {
#ifdef Q_OS_ANDROID
  const auto models_dir_result = AndroidModelsExtractDir();
  if (!models_dir_result) {
    CLIENT_WARN("Failed to get Android models directory");
    return config;
  }

  const auto& models_dir = *models_dir_result;
  ModelConfig resolved = config;

  // Resolve model path
  const auto model_filename = QString::fromStdString(config.model_path.filename().string());
  if (model_filename == "face_detection_yunet_2023mar.onnx") {
    resolved.model_path = QDir(models_dir).filePath("face_detection_yunet_2023mar.onnx").toStdString();
  } else if (model_filename == "res10_300x300_ssd_iter_140000.caffemodel") {
    resolved.model_path = QDir(models_dir).filePath("res10_300x300_ssd_iter_140000.caffemodel").toStdString();
  }

  // Resolve config path if present
  if (!config.config_path.empty()) {
    const auto config_filename = QString::fromStdString(config.config_path.filename().string());
    if (config_filename == "res10_300x300_ssd_deploy.prototxt") {
      resolved.config_path = QDir(models_dir).filePath("res10_300x300_ssd_deploy.prototxt").toStdString();
    } else if (config_filename == "res10_300x300_ssd_deploy_broken.prototxt") {
      resolved.config_path = QDir(models_dir).filePath("res10_300x300_ssd_deploy_broken.prototxt").toStdString();
    }
  }

  return resolved;
#else
  return config;
#endif
}

AppConfig ParseArguments(int argc, char** argv) {
  auto config = AppConfig::Default();

  // Create a temporary QCoreApplication for argument parsing
  // This is needed because QCommandLineParser requires a QCoreApplication instance
  QCoreApplication temp_app(argc, argv);
  temp_app.setApplicationName(QString::fromUtf8(App::Name().data(), static_cast<qsizetype>(App::Name().size())));
  temp_app.setApplicationVersion(
      QString::fromUtf8(App::Version().data(), static_cast<qsizetype>(App::Version().size())));

  QCommandLineParser parser;
  parser.setApplicationDescription(QStringLiteral("Face tracking client application"));
  parser.addHelpOption();
  parser.addVersionOption();

  // Define command line options
  QCommandLineOption headlessOption(QStringLiteral("headless"), QStringLiteral("Run without GUI"));
  parser.addOption(headlessOption);

  QCommandLineOption verboseOption({QStringLiteral("V"), QStringLiteral("verbose")},
                                   QStringLiteral("Enable verbose logging"));
  parser.addOption(verboseOption);

  QCommandLineOption gpuOption(QStringLiteral("gpu"), QStringLiteral("Use GPU acceleration"));
  parser.addOption(gpuOption);

  QCommandLineOption maxFramesOption(QStringLiteral("max-frames"),
                                     QStringLiteral("Stop after N frames (0 = unlimited)"), QStringLiteral("frames"),
                                     QStringLiteral("0"));
  parser.addOption(maxFramesOption);

  QCommandLineOption modelTypeOption(QStringLiteral("model-type"),
                                     QStringLiteral("Face detection model type: yunet, resnet10, mobilenet"),
                                     QStringLiteral("type"), QStringLiteral("yunet"));
  parser.addOption(modelTypeOption);

  QCommandLineOption modelOption(QStringLiteral("model"),
                                 QStringLiteral("Path to face detection model (overrides --model-type)"),
                                 QStringLiteral("path"));
  parser.addOption(modelOption);

  QCommandLineOption configOption(QStringLiteral("config"), QStringLiteral("Path to model configuration"),
                                  QStringLiteral("path"));
  parser.addOption(configOption);

  QCommandLineOption cameraOption(QStringLiteral("camera"), QStringLiteral("Camera device ID"),
                                  QStringLiteral("device"));
  parser.addOption(cameraOption);

  QCommandLineOption confidenceOption(QStringLiteral("confidence"),
                                      QStringLiteral("Detection confidence threshold (0.0-1.0)"),
                                      QStringLiteral("value"), QStringLiteral("0.5"));
  parser.addOption(confidenceOption);

  QCommandLineOption widthOption(QStringLiteral("width"), QStringLiteral("Preferred camera width"),
                                 QStringLiteral("pixels"), QStringLiteral("640"));
  parser.addOption(widthOption);

  QCommandLineOption heightOption(QStringLiteral("height"), QStringLiteral("Preferred camera height"),
                                  QStringLiteral("pixels"), QStringLiteral("480"));
  parser.addOption(heightOption);

  QCommandLineOption fpsOption(QStringLiteral("fps"), QStringLiteral("Preferred camera FPS"), QStringLiteral("rate"),
                               QStringLiteral("30"));
  parser.addOption(fpsOption);

  // Parse arguments
  parser.process(temp_app);

  config.headless = parser.isSet(headlessOption);
  config.verbose = parser.isSet(verboseOption);

  bool ok = false;
  config.max_frames = parser.value(maxFramesOption).toUInt(&ok);
  if (!ok) {
    CLIENT_WARN("Invalid max-frames value, using default (0)");
    config.max_frames = 0;
  }

  // Parse model type or custom model path
  const QString custom_model_path = parser.value(modelOption);
  if (!custom_model_path.isEmpty()) {
    // Custom model path specified
    config.face_tracker.model_path = custom_model_path.toStdString();
    config.model_type = ModelType::kCustom;

    const QString config_path = parser.value(configOption);
    if (!config_path.isEmpty()) {
      config.face_tracker.config_path = config_path.toStdString();
    }
  } else {
    // Use predefined model type
    const QString model_type_str = parser.value(modelTypeOption);
    auto model_type = ModelType::kYuNetONNX;

    if (model_type_str == QStringLiteral("yunet")) {
      model_type = ModelType::kYuNetONNX;
    } else if (model_type_str == QStringLiteral("resnet10")) {
      model_type = ModelType::kResNet10Caffe;
    } else if (model_type_str == QStringLiteral("mobilenet")) {
      model_type = ModelType::kMobileNetCaffe;
    } else {
      CLIENT_WARN("Unknown model type '{}', using default (yunet)", model_type_str.toStdString());
    }

    const auto model_config = ModelConfig::FromType(model_type);
    config.face_tracker = FaceTrackerConfig::FromModelConfig(model_config);
    config.model_type = model_type;
  }

  config.face_tracker.use_gpu = parser.isSet(gpuOption);

  config.face_tracker.confidence_threshold = parser.value(confidenceOption).toFloat(&ok);
  if (!ok || config.face_tracker.confidence_threshold < 0.0F || config.face_tracker.confidence_threshold > 1.0F) {
    CLIENT_WARN("Invalid confidence value, using default (0.5)");
    config.face_tracker.confidence_threshold = 0.5F;
  }

  CLIENT_ASSERT(config.face_tracker.confidence_threshold >= 0.0F && config.face_tracker.confidence_threshold <= 1.0F,
                "Confidence threshold must be in [0, 1] range");

  const QString camera_id = parser.value(cameraOption);
  if (!camera_id.isEmpty()) {
    config.camera.device_id = camera_id.toStdString();
  }

  config.camera.preferred_width = parser.value(widthOption).toInt(&ok);
  if (!ok || config.camera.preferred_width <= 0) {
    CLIENT_WARN("Invalid width value, using default (640)");
    config.camera.preferred_width = 640;
  }

  config.camera.preferred_height = parser.value(heightOption).toInt(&ok);
  if (!ok || config.camera.preferred_height <= 0) {
    CLIENT_WARN("Invalid height value, using default (480)");
    config.camera.preferred_height = 480;
  }

  config.camera.preferred_fps = parser.value(fpsOption).toInt(&ok);
  if (!ok || config.camera.preferred_fps <= 0) {
    CLIENT_WARN("Invalid fps value, using default (30)");
    config.camera.preferred_fps = 30;
  }

  CLIENT_ASSERT(config.camera.preferred_width > 0, "Camera width must be positive");
  CLIENT_ASSERT(config.camera.preferred_height > 0, "Camera height must be positive");
  CLIENT_ASSERT(config.camera.preferred_fps > 0, "Camera FPS must be positive");

  return config;
}

App::App(int argc, char** argv, AppConfig config, bool use_gui)
    : config_(std::move(config)),
      use_gui_(use_gui || !config_.headless),
      last_fps_update_(std::chrono::steady_clock::now()) {
  // WORKAROUND for Qt 6.10.1 bug: QCoreApplication::arguments() crashes when
  // accessing argv pointers. Create persistent copies of argc/argv to ensure
  // they remain valid throughout the application lifetime.
  static std::vector<std::string> arg_storage;
  static std::vector<char*> arg_ptrs;
  static int static_argc = 0;

  if (arg_storage.empty()) {
    arg_storage.reserve(static_cast<size_t>(argc));
    arg_ptrs.reserve(static_cast<size_t>(argc) + 1);

    for (int i = 0; i < argc; ++i) {
      arg_storage.emplace_back(argv[i]);
      arg_ptrs.push_back(arg_storage.back().data());
    }
    arg_ptrs.push_back(nullptr);  // argv must be null-terminated
    static_argc = argc;
  }

  // Create appropriate Qt application
  if (use_gui_) {
    qt_app_ = std::make_unique<QApplication>(static_argc, arg_ptrs.data());
    // Set QML style to avoid native style customization warnings
    QQuickStyle::setStyle("Basic");
  } else {
    qt_app_ = std::make_unique<QCoreApplication>(static_argc, arg_ptrs.data());
  }

  qt_app_->setApplicationName(QString::fromUtf8(Name().data(), static_cast<qsizetype>(Name().size())));
  qt_app_->setApplicationVersion(QString::fromUtf8(Version().data(), static_cast<qsizetype>(Version().size())));

  CLIENT_INFO("{} v{} initializing... (GUI: {})", Name(), Version(), use_gui_ ? "enabled" : "disabled");
}

App::~App() {
  if (running_) {
    RequestStop();
  }

  if (camera_.Initialized()) {
    camera_.Stop();
  }

  if (gui_window_) {
    gui_window_->close();
    gui_window_.reset();
  }

  CLIENT_INFO("{} shutting down", Name());
}

AppReturnCode App::Run() {
  const auto init_result = Initialize();
  if (!init_result) {
    return init_result.error();
  }

  running_.store(true, std::memory_order_release);
  CLIENT_INFO("{} started", Name());

  // Create GUI if enabled
  if (use_gui_) {
    gui_window_ = std::make_unique<GuiWindow>();

    // Initialize QML engine (may fail and fall back to headless mode)
    const bool gui_initialized = gui_window_->Initialize();
    if (!gui_initialized) {
      CLIENT_WARN("GUI initialization failed, continuing in headless mode");
      // Keep gui_window_ for programmatic updates, but don't show it
    }

    // Set up GUI callbacks
    gui_window_->SetCameraSwitchCallback([this](std::string_view device_id) {
      const auto result = SwitchCamera(device_id);
      if (!result) {
        CLIENT_ERROR("Failed to switch camera from GUI");
      }
    });

    gui_window_->SetModelSwitchCallback([this](ModelType model_type) {
      const auto result = SwitchModel(model_type);
      if (!result) {
        CLIENT_ERROR("Failed to switch model from GUI");
      }
    });

    gui_window_->SetSettingsChangedCallback([this](const QVariantMap& settings) {
      CLIENT_INFO("Settings changed from GUI: {} setting(s)", settings.size());

      for (auto it = settings.constBegin(); it != settings.constEnd(); ++it) {
        const auto key = it.key().toStdString();
        const auto& value = it.value();

        CLIENT_INFO("Setting: {} = {}", key, value.toString().toStdString());

        // Handle FPS target
        if (key == "fps") {
          const int fps = value.toInt();
          config_.camera.preferred_fps = fps;
          camera_.UpdateConfig(config_.camera);
          CLIENT_INFO("Updated target FPS to {}", fps);
        }

        // Handle throttling
        else if (key == "throttling") {
          config_.camera.enable_throttling = value.toBool();
          camera_.UpdateConfig(config_.camera);
          CLIENT_INFO("Frame throttling {}", config_.camera.enable_throttling ? "enabled" : "disabled");
        }

        // Handle resolution (requires camera restart)
        else if (key == "width") {
          config_.camera.preferred_width = value.toInt();
          CLIENT_INFO("Resolution width set to {} (restart camera to apply)", config_.camera.preferred_width);
        } else if (key == "height") {
          config_.camera.preferred_height = value.toInt();
          CLIENT_INFO("Resolution height set to {} (restart camera to apply)", config_.camera.preferred_height);
        }

        // Handle GPU acceleration
        else if (key == "gpu") {
          const bool use_gpu = value.toBool();
          if (config_.face_tracker.use_gpu != use_gpu) {
            config_.face_tracker.use_gpu = use_gpu;
            CLIENT_INFO("GPU {} (reloading model...)", use_gpu ? "enabled" : "disabled");

            auto model_config = ModelConfig::FromType(config_.model_type);
            model_config.use_gpu = use_gpu;

            const auto result = face_tracker_.Reinitialize(model_config);
            if (!result) {
              CLIENT_ERROR("Failed to update GPU: {}", FaceTrackerErrorToString(result.error()));
            } else {
              CLIENT_INFO("Model reloaded with GPU {}", use_gpu ? "ON" : "OFF");
            }
          }
        }

        // Handle confidence threshold
        else if (key == "confidence") {
          config_.face_tracker.confidence_threshold = value.toFloat();
          face_tracker_.SetConfidenceThreshold(config_.face_tracker.confidence_threshold);
          CLIENT_INFO("Confidence threshold: {:.2f}", config_.face_tracker.confidence_threshold);
        }

        // Handle NMS threshold
        else if (key == "nms") {
          config_.face_tracker.nms_threshold = value.toFloat();
          face_tracker_.SetNmsThreshold(config_.face_tracker.nms_threshold);
          CLIENT_INFO("NMS threshold: {:.2f}", config_.face_tracker.nms_threshold);
        }

        // Handle verbose logging
        else if (key == "verbose") {
          config_.verbose = value.toBool();
          CLIENT_INFO("Verbose logging {}", config_.verbose ? "enabled" : "disabled");
        }
      }
    });

    // Initialize Bluetooth
    const auto bt_init = bluetooth_.Initialize();
    if (!bt_init) {
      CLIENT_WARN("Bluetooth initialization failed: {}", comm::BluetoothErrorToString(bt_init.error()));
    } else {
      CLIENT_INFO("Bluetooth initialized successfully");

      // Set up Bluetooth state callback
      bluetooth_.SetStateCallback([this](comm::BluetoothState state, std::string_view error_message) {
        if (config_.verbose) {
          CLIENT_INFO("Bluetooth state changed: {} {}", comm::BluetoothStateToString(state),
                      error_message.empty() ? "" : std::string("- ") + std::string(error_message));
        }

        // Update GUI connection state
        if (gui_window_) {
          ConnectionState gui_state = ConnectionState::kDisconnected;
          switch (state) {
            case comm::BluetoothState::kDisconnected:
              gui_state = ConnectionState::kDisconnected;
              break;
            case comm::BluetoothState::kScanning:
              gui_state = ConnectionState::kDisconnected;
              break;
            case comm::BluetoothState::kConnecting:
              gui_state = ConnectionState::kConnecting;
              break;
            case comm::BluetoothState::kConnected:
              gui_state = ConnectionState::kConnected;
              break;
            case comm::BluetoothState::kError:
              gui_state = ConnectionState::kError;
              break;
          }
          gui_window_->SetConnectionState(gui_state, std::string(error_message));
        }
      });

      // Set up device discovered callback
      bluetooth_.SetDeviceDiscoveredCallback([this](const comm::BluetoothDevice& device) {
        if (config_.verbose) {
          CLIENT_INFO("Bluetooth device discovered: {} ({}), RSSI: {} dBm, paired: {}, connected: {}", device.name,
                      device.address, device.rssi, device.is_paired, device.is_connected);
        }
      });

      // Set up scan complete callback
      bluetooth_.SetScanCompleteCallback([this](std::span<const comm::BluetoothDevice> devices) {
        CLIENT_INFO("Bluetooth scan complete: {} device(s) found", devices.size());

        if (config_.verbose) {
          for (const auto& device : devices) {
            CLIENT_INFO("  - {} ({}) - RSSI: {} dBm, paired: {}, connected: {}", device.name, device.address,
                        device.rssi, device.is_paired, device.is_connected);
          }
        }

        // Update GUI with discovered devices
        if (gui_window_) {
          std::vector<BluetoothDeviceInfo> gui_devices;
          gui_devices.reserve(devices.size());
          for (const auto& device : devices) {
            gui_devices.push_back({.name = device.name, .address = device.address});
          }
          gui_window_->UpdateAvailableDevices(gui_devices);
        }
      });

      // Set up data received callback
      bluetooth_.SetDataReceivedCallback([this](std::span<const uint8_t> data) {
        if (config_.verbose) {
          CLIENT_INFO("Received {} bytes from Bluetooth device", data.size());
        }
        // TODO: Handle received data (e.g., status updates from ESP32)
      });
    }

    // Set up GUI Bluetooth callbacks
    gui_window_->SetScanCallback([this]() {
      CLIENT_INFO("Starting Bluetooth scan...");
      const auto result = bluetooth_.StartScan(10000);  // 10 second timeout
      if (!result) {
        CLIENT_ERROR("Failed to start Bluetooth scan: {}", comm::BluetoothErrorToString(result.error()));
        if (gui_window_) {
          gui_window_->SetConnectionState(ConnectionState::kError,
                                          std::string(comm::BluetoothErrorToString(result.error())));
        }
      } else if (config_.verbose) {
        CLIENT_INFO("Bluetooth scan started (available: {}, enabled: {})", bluetooth_.Available(),
                    bluetooth_.Enabled());
      }
    });

    gui_window_->SetConnectCallback([this](std::string_view address) {
      CLIENT_INFO("Attempting to connect to Bluetooth device: {}", address);
      const auto result = bluetooth_.Connect(address);
      if (!result) {
        CLIENT_ERROR("Failed to connect to Bluetooth device: {}", comm::BluetoothErrorToString(result.error()));
        if (gui_window_) {
          gui_window_->SetConnectionState(ConnectionState::kError,
                                          std::string(comm::BluetoothErrorToString(result.error())));
        }
      } else {
        if (config_.verbose) {
          CLIENT_INFO("Connection initiated to {}", address);
        }

        // Start camera once Bluetooth is connected
        if (!camera_.Active()) {
          CLIENT_INFO("Starting camera after Bluetooth connection...");
          const auto start_result = camera_.Start();
          if (!start_result) {
            CLIENT_ERROR("Failed to start camera: {}", CameraErrorToString(start_result.error()));
          } else {
            CLIENT_INFO("Camera started successfully");
          }
        }

        // Start calibration automatically after connection
        CLIENT_INFO("Starting automatic calibration...");
        const auto calibrate_result = bluetooth_.SendCalibrate();
        if (!calibrate_result) {
          CLIENT_ERROR("Failed to send calibration command: {}",
                       comm::BluetoothErrorToString(calibrate_result.error()));
        } else {
          CLIENT_INFO("Calibration command sent");
        }
      }
    });

    gui_window_->SetDisconnectCallback([this]() {
      CLIENT_INFO("Disconnecting from Bluetooth device...");

      // Stop camera when disconnecting
      if (camera_.Active()) {
        CLIENT_INFO("Stopping camera before disconnect...");
        camera_.Stop();
      }

      const auto result = bluetooth_.Disconnect();
      if (!result) {
        CLIENT_ERROR("Failed to disconnect from Bluetooth device: {}", comm::BluetoothErrorToString(result.error()));
      } else if (config_.verbose) {
        CLIENT_INFO("Disconnected successfully");
      }
    });

    gui_window_->SetCalibrateCallback([this]() {
      CLIENT_INFO("Manual calibration requested");
      const auto result = bluetooth_.SendCalibrate();
      if (!result) {
        CLIENT_ERROR("Failed to send calibration command: {}", comm::BluetoothErrorToString(result.error()));
      } else {
        CLIENT_INFO("Calibration command sent successfully");
      }
    });

    QObject::connect(gui_window_.get(), &GuiWindow::QuitRequested, [this]() { RequestStop(); });

    // Update camera list in GUI
    const auto cameras = Camera::AvailableDevices();
    const auto current_camera = camera_.CurrentDevice();
    const std::string current_id = current_camera ? current_camera->id : "";
    gui_window_->UpdateCameraList(cameras, current_id);

    // Set current model in GUI
    gui_window_->SetCurrentModel(config_.model_type);

    gui_window_->show();
    CLIENT_INFO("GUI window displayed");
  }

  // Set up frame processing callback
  camera_.SetFrameCallback([this](const Frame& frame) { ProcessFrame(frame); });

#ifdef Q_OS_ANDROID
  // Request camera permission on Android before starting camera
  CLIENT_INFO("Requesting camera permission on Android...");
  QCameraPermission camera_permission;

  switch (qt_app_->checkPermission(camera_permission)) {
    case Qt::PermissionStatus::Undetermined:
      CLIENT_INFO("Camera permission undetermined, requesting...");
      qt_app_->requestPermission(camera_permission, [](const QPermission& permission) {
        if (permission.status() == Qt::PermissionStatus::Granted) {
          CLIENT_INFO("Camera permission granted");
        } else {
          CLIENT_WARN("Camera permission denied by user");
        }
      });
      // Wait for permission response by processing events
      for (int i = 0; i < 100 && qt_app_->checkPermission(camera_permission) == Qt::PermissionStatus::Undetermined;
           ++i) {
        qt_app_->processEvents(QEventLoop::AllEvents, 100);
      }
      break;
    case Qt::PermissionStatus::Denied:
      CLIENT_WARN("Camera permission was previously denied");
      break;
    case Qt::PermissionStatus::Granted:
      CLIENT_INFO("Camera permission already granted");
      break;
  }

  if (qt_app_->checkPermission(camera_permission) != Qt::PermissionStatus::Granted) {
    CLIENT_ERROR("Camera permission not granted - camera features will not work");
  }
#endif

  // Don't start camera here - wait for Bluetooth connection (unless headless mode)
  if (!use_gui_) {
    // In headless mode, start camera immediately since there's no Bluetooth UI
    const auto start_result = camera_.Start();
    if (!start_result) {
      CLIENT_ERROR("Failed to start camera: {}", CameraErrorToString(start_result.error()));
      running_ = false;
      return AppReturnCode::kCameraInitFailed;
    }
    CLIENT_INFO("Camera started (headless mode)");
  } else {
    CLIENT_INFO("Camera will start after Bluetooth connection is established");
  }

  // Process frames using Qt event loop with a timer
  QTimer process_timer;
  process_timer.setInterval(1);  // Process as fast as possible

  QObject::connect(&process_timer, &QTimer::timeout, [this]() {
    // Check if we should stop
    if (stop_requested_.load(std::memory_order_acquire)) {
      qt_app_->quit();
      return;
    }

    // Check frame limit
    const uint64_t frames = frames_processed_.load(std::memory_order_relaxed);
    if (config_.max_frames > 0 && frames >= config_.max_frames) {
      CLIENT_INFO("Reached frame limit ({}), stopping", config_.max_frames);
      qt_app_->quit();
      return;
    }

    // Process Qt events (this will trigger frame callbacks)
    qt_app_->processEvents();
  });

  process_timer.start();

  // Run the Qt event loop
  int result = qt_app_->exec();

  running_.store(false, std::memory_order_release);
  camera_.Stop();

  CLIENT_INFO("{} finished, processed {} frames", Name(), frames_processed_.load(std::memory_order_relaxed));

  return result == 0 ? AppReturnCode::kSuccess : AppReturnCode::kUnknownError;
}

auto App::SwitchModel(ModelType model_type) -> std::expected<void, AppReturnCode> {
  if (!running_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot switch model: application not running");
    return std::unexpected(AppReturnCode::kUnknownError);
  }

  CLIENT_INFO("Switching to model: {}", ModelTypeToString(model_type));

  auto model_config = ModelConfig::FromType(model_type);

  // Resolve model paths for Android
  model_config = ResolveModelConfigForAndroid(model_config);

  // Validate model files exist
  if (!model_config.Validate()) {
    CLIENT_ERROR("Model validation failed: files not found for {}", ModelTypeToString(model_type));
    return std::unexpected(AppReturnCode::kFaceTrackerInitFailed);
  }

  // Reinitialize face tracker with new model
  const auto result = face_tracker_.Reinitialize(model_config);
  if (!result) {
    CLIENT_ERROR("Failed to reinitialize face tracker: {}", FaceTrackerErrorToString(result.error()));
    return std::unexpected(AppReturnCode::kFaceTrackerInitFailed);
  }

  // Update configuration
  config_.face_tracker = FaceTrackerConfig::FromModelConfig(model_config);
  config_.model_type = model_type;

  CLIENT_INFO("Successfully switched to model: {}", ModelTypeToString(model_type));
  return {};
}

auto App::SwitchCamera(std::string_view device_id) -> std::expected<void, AppReturnCode> {
  if (!running_.load(std::memory_order_acquire)) {
    CLIENT_ERROR("Cannot switch camera: application not running");
    return std::unexpected(AppReturnCode::kUnknownError);
  }

  CLIENT_INFO("Switching to camera: {}", device_id.empty() ? "default" : device_id);

  const auto result = camera_.SwitchCamera(device_id);
  if (!result) {
    CLIENT_ERROR("Failed to switch camera: {}", CameraErrorToString(result.error()));
    return std::unexpected(AppReturnCode::kCameraInitFailed);
  }

  // Update configuration
  config_.camera.device_id = std::string(device_id);

  CLIENT_INFO("Successfully switched camera");
  return {};
}

auto App::Initialize() -> std::expected<void, AppReturnCode> {
  CLIENT_ASSERT(!running_.load(std::memory_order_acquire), "Cannot initialize while running");
  CLIENT_ASSERT(!camera_.Initialized(), "Camera already initialized");
  CLIENT_ASSERT(!face_tracker_.Initialized(), "Face tracker already initialized");

  // Initialize camera
  const auto camera_result = camera_.Initialize(config_.camera);
  if (!camera_result) {
    CLIENT_ERROR("Failed to initialize camera: {}", CameraErrorToString(camera_result.error()));
    return std::unexpected(AppReturnCode::kCameraInitFailed);
  }

  CLIENT_ASSERT(camera_.Initialized(), "Camera should be initialized after successful Initialize()");

  // Initialize face tracker
  const auto tracker_result = face_tracker_.Initialize(config_.face_tracker);
  if (!tracker_result) {
    CLIENT_ERROR("Failed to initialize face tracker: {}", FaceTrackerErrorToString(tracker_result.error()));
    return std::unexpected(AppReturnCode::kFaceTrackerInitFailed);
  }

  CLIENT_ASSERT(face_tracker_.Initialized(), "Face tracker should be initialized after successful Initialize()");

  CLIENT_INFO("App initialized successfully");
  return {};
}

void App::ProcessFrame(const Frame& frame) {
  CLIENT_ASSERT(running_.load(std::memory_order_acquire), "ProcessFrame called while not running");
  CLIENT_ASSERT(face_tracker_.Initialized(), "Face tracker must be initialized");

  if (frame.Empty()) [[unlikely]] {
    return;
  }

  // Run face detection
  auto result = face_tracker_.Detect(frame);
  if (!result) {
    if (config_.verbose) {
      CLIENT_WARN("Face detection failed: {}", FaceTrackerErrorToString(result.error()));
    }
    return;
  }

  frames_processed_.fetch_add(1, std::memory_order_relaxed);

  HandleDetection(*result, frame);
}

void App::HandleDetection(const FaceDetectionResult& result, const Frame& frame) {
  CLIENT_ASSERT(running_.load(std::memory_order_acquire), "HandleDetection called while not running");

  {
    std::scoped_lock lock(detection_mutex_);
    last_detection_ = result;
  }

  // Update GUI if enabled
  if (use_gui_) {
    UpdateGui();
  }

  if (config_.verbose && result.HasFaces()) {
    CLIENT_INFO("Frame {}: Detected {} face(s) in {:.2f}ms", result.frame_id, result.FaceCount(),
                result.processing_time_ms);

    for (size_t i = 0; i < result.faces.size(); ++i) {
      const auto& face = result.faces[i];
      CLIENT_ASSERT(face.confidence >= 0.0F && face.confidence <= 1.0F, "Face confidence must be in [0, 1] range");
      CLIENT_INFO("  Face {}: bbox=({:.1f}, {:.1f}, {:.1f}, {:.1f}), conf={:.2f}, dist={:.2f}, priority={:.2f}", i,
                  face.bounding_box.x, face.bounding_box.y, face.bounding_box.width, face.bounding_box.height,
                  face.confidence, face.relative_distance, face.Priority());
    }
  }

  // Send servo commands if connected and faces detected
  if (bluetooth_.State() == comm::BluetoothState::kConnected && result.HasFaces()) {
    // Get the primary face (highest priority)
    const auto primary_face_opt = result.HighestPriorityFace();
    if (!primary_face_opt) {
      return;
    }

    const auto& primary_face = *primary_face_opt;

    // Calculate pan and tilt angles based on face position
    // Face position is in pixels, normalize to [-1, 1] range where center is 0
    const float frame_center_x = static_cast<float>(frame.Width()) / 2.0F;
    const float frame_center_y = static_cast<float>(frame.Height()) / 2.0F;

    const float face_center_x = primary_face.bounding_box.x + primary_face.bounding_box.width / 2.0F;
    const float face_center_y = primary_face.bounding_box.y + primary_face.bounding_box.height / 2.0F;

    // Normalized offset from center [-1, 1]
    const float offset_x = (face_center_x - frame_center_x) / frame_center_x;
    const float offset_y = (face_center_y - frame_center_y) / frame_center_y;

    // Convert to servo angles (pan: -90 to 90, tilt: -45 to 45)
    const float pan_angle = offset_x * 90.0F;
    const float tilt_angle = offset_y * 45.0F;

    comm::ServoCommand cmd{.pan_angle = pan_angle, .tilt_angle = tilt_angle, .speed = 1.0F, .smooth = true};

    const auto send_result = bluetooth_.SendCommand(cmd);
    if (!send_result && config_.verbose) {
      CLIENT_ERROR("Failed to send servo command: {}", comm::BluetoothErrorToString(send_result.error()));
    }
  }

  // Call user callback if set
  if (detection_callback_) {
    detection_callback_(result);
  }
}

void App::UpdateGui() {
  if (!gui_window_ || !running_.load(std::memory_order_acquire)) {
    return;
  }

  // Calculate FPS
  std::scoped_lock lock(gui_mutex_);
  const auto now = std::chrono::steady_clock::now();
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fps_update_).count();

  ++fps_frame_count_;

  if (elapsed >= 1000) {  // Update every second
    current_fps_ = static_cast<float>(fps_frame_count_) * 1000.0F / static_cast<float>(elapsed);
    fps_frame_count_ = 0;
    last_fps_update_ = now;
  }

  // Get the last frame from camera
  const auto frame_result = camera_.CaptureFrame();
  if (!frame_result) {
    return;
  }

  // Get last detection safely
  std::optional<FaceDetectionResult> detection_copy;
  {
    std::scoped_lock det_lock(detection_mutex_);
    detection_copy = last_detection_;
  }

  // Update frame with detection overlay
  gui_window_->UpdateFrame(*frame_result, detection_copy);

  // Update statistics
  const size_t face_count = detection_copy ? detection_copy->faces.size() : 0;
  const uint64_t frames = frames_processed_.load(std::memory_order_relaxed);
  gui_window_->UpdateStats(current_fps_, frames, face_count);
}

}  // namespace client
