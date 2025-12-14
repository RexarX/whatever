#pragma once

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

namespace client {

/**
 * @brief Manages persistent application settings using QSettings.
 * @details Provides a centralized interface for storing and retrieving
 * application settings with platform-specific storage (registry on Windows,
 * .config files on Linux, plist on macOS).
 */
class SettingsManager final : public QObject {
  Q_OBJECT

  Q_PROPERTY(int targetFps READ targetFps WRITE setTargetFps NOTIFY targetFpsChanged)
  Q_PROPERTY(bool throttlingEnabled READ throttlingEnabled WRITE setThrottlingEnabled NOTIFY throttlingEnabledChanged)
  Q_PROPERTY(int resolutionWidth READ resolutionWidth WRITE setResolutionWidth NOTIFY resolutionChanged)
  Q_PROPERTY(int resolutionHeight READ resolutionHeight WRITE setResolutionHeight NOTIFY resolutionChanged)
  Q_PROPERTY(
      float confidenceThreshold READ confidenceThreshold WRITE setConfidenceThreshold NOTIFY confidenceThresholdChanged)
  Q_PROPERTY(float nmsThreshold READ nmsThreshold WRITE setNmsThreshold NOTIFY nmsThresholdChanged)
  Q_PROPERTY(bool gpuEnabled READ gpuEnabled WRITE setGpuEnabled NOTIFY gpuEnabledChanged)
  Q_PROPERTY(bool verboseLogging READ verboseLogging WRITE setVerboseLogging NOTIFY verboseLoggingChanged)
  Q_PROPERTY(bool darkMode READ darkMode WRITE setDarkMode NOTIFY darkModeChanged)
  Q_PROPERTY(bool showBoundingBoxes READ showBoundingBoxes WRITE setShowBoundingBoxes NOTIFY displayOptionsChanged)
  Q_PROPERTY(bool showConfidence READ showConfidence WRITE setShowConfidence NOTIFY displayOptionsChanged)
  Q_PROPERTY(bool showDistance READ showDistance WRITE setShowDistance NOTIFY displayOptionsChanged)
  Q_PROPERTY(
      bool cameraPreviewVisible READ cameraPreviewVisible WRITE setCameraPreviewVisible NOTIFY displayOptionsChanged)
  Q_PROPERTY(QString lastCameraId READ lastCameraId WRITE setLastCameraId NOTIFY lastCameraIdChanged)
  Q_PROPERTY(int lastModelType READ lastModelType WRITE setLastModelType NOTIFY lastModelTypeChanged)

public:
  explicit SettingsManager(QObject* parent = nullptr);
  ~SettingsManager() override = default;

  SettingsManager(const SettingsManager&) = delete;
  SettingsManager(SettingsManager&&) = delete;
  SettingsManager& operator=(const SettingsManager&) = delete;
  SettingsManager& operator=(SettingsManager&&) = delete;

  // Camera settings
  [[nodiscard]] int targetFps() const noexcept { return target_fps_; }
  [[nodiscard]] bool throttlingEnabled() const noexcept { return throttling_enabled_; }
  [[nodiscard]] int resolutionWidth() const noexcept { return resolution_width_; }
  [[nodiscard]] int resolutionHeight() const noexcept { return resolution_height_; }

  // Detection settings
  [[nodiscard]] float confidenceThreshold() const noexcept { return confidence_threshold_; }
  [[nodiscard]] float nmsThreshold() const noexcept { return nms_threshold_; }

  // Processing settings
  [[nodiscard]] bool gpuEnabled() const noexcept { return gpu_enabled_; }
  [[nodiscard]] bool verboseLogging() const noexcept { return verbose_logging_; }

  // Display settings
  [[nodiscard]] bool darkMode() const noexcept { return dark_mode_; }
  [[nodiscard]] bool showBoundingBoxes() const noexcept { return show_bounding_boxes_; }
  [[nodiscard]] bool showConfidence() const noexcept { return show_confidence_; }
  [[nodiscard]] bool showDistance() const noexcept { return show_distance_; }
  [[nodiscard]] bool cameraPreviewVisible() const noexcept { return camera_preview_visible_; }

  // Last used settings
  [[nodiscard]] QString lastCameraId() const noexcept { return last_camera_id_; }
  [[nodiscard]] int lastModelType() const noexcept { return last_model_type_; }

  // Setters
  void setTargetFps(int fps) noexcept;
  void setThrottlingEnabled(bool enabled) noexcept;
  void setResolutionWidth(int width) noexcept;
  void setResolutionHeight(int height) noexcept;
  void setConfidenceThreshold(float threshold) noexcept;
  void setNmsThreshold(float threshold) noexcept;
  void setGpuEnabled(bool enabled) noexcept;
  void setVerboseLogging(bool enabled) noexcept;
  void setDarkMode(bool enabled) noexcept;
  void setShowBoundingBoxes(bool show) noexcept;
  void setShowConfidence(bool show) noexcept;
  void setShowDistance(bool show) noexcept;
  void setCameraPreviewVisible(bool visible) noexcept;
  void setLastCameraId(const QString& id) noexcept;
  void setLastModelType(int type) noexcept;

  /**
   * @brief Loads all settings from persistent storage.
   */
  Q_INVOKABLE void load();

  /**
   * @brief Saves all settings to persistent storage.
   */
  Q_INVOKABLE void save();

  /**
   * @brief Resets all settings to default values.
   */
  Q_INVOKABLE void resetToDefaults();

signals:
  void targetFpsChanged();
  void throttlingEnabledChanged();
  void resolutionChanged();
  void confidenceThresholdChanged();
  void nmsThresholdChanged();
  void gpuEnabledChanged();
  void verboseLoggingChanged();
  void darkModeChanged();
  void displayOptionsChanged();
  void lastCameraIdChanged();
  void lastModelTypeChanged();

private:
  QSettings settings_;

  // Camera settings
  int target_fps_{30};
  bool throttling_enabled_{true};
  int resolution_width_{640};
  int resolution_height_{480};

  // Detection settings
  float confidence_threshold_{0.5F};
  float nms_threshold_{0.4F};

  // Processing settings
  bool gpu_enabled_{false};
  bool verbose_logging_{false};

  // Display settings
  bool dark_mode_{false};
  bool show_bounding_boxes_{true};
  bool show_confidence_{true};
  bool show_distance_{true};
  bool camera_preview_visible_{true};

  // Last used settings
  QString last_camera_id_;
  int last_model_type_{0};  // 0 = YuNet
};

}  // namespace client
