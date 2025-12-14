#include <client/app/settings_manager.hpp>

#include <client/core/logger.hpp>

namespace client {

SettingsManager::SettingsManager(QObject* parent) : QObject(parent), settings_("FaceTracker", "FaceTrackerClient") {
  CLIENT_INFO("SettingsManager created");
  load();
}

void SettingsManager::load() {
  CLIENT_INFO("Loading settings from storage...");

  // Camera settings
  target_fps_ = settings_.value("camera/targetFps", 30).toInt();
  throttling_enabled_ = settings_.value("camera/throttlingEnabled", true).toBool();
  resolution_width_ = settings_.value("camera/resolutionWidth", 640).toInt();
  resolution_height_ = settings_.value("camera/resolutionHeight", 480).toInt();

  // Detection settings
  confidence_threshold_ = settings_.value("detection/confidenceThreshold", 0.5).toFloat();
  nms_threshold_ = settings_.value("detection/nmsThreshold", 0.4).toFloat();

  // Processing settings
  gpu_enabled_ = settings_.value("processing/gpuEnabled", false).toBool();
  verbose_logging_ = settings_.value("processing/verboseLogging", false).toBool();

  // Display settings
  dark_mode_ = settings_.value("display/darkMode", false).toBool();
  show_bounding_boxes_ = settings_.value("display/showBoundingBoxes", true).toBool();
  show_confidence_ = settings_.value("display/showConfidence", true).toBool();
  show_distance_ = settings_.value("display/showDistance", true).toBool();
  camera_preview_visible_ = settings_.value("display/cameraPreviewVisible", true).toBool();

  // Last used settings
  last_camera_id_ = settings_.value("lastUsed/cameraId", "").toString();
  last_model_type_ = settings_.value("lastUsed/modelType", 0).toInt();

  CLIENT_INFO("Settings loaded: FPS={}, Resolution={}x{}, GPU={}, DarkMode={}", target_fps_, resolution_width_,
              resolution_height_, gpu_enabled_, dark_mode_);

  // Emit all changed signals to update UI
  emit targetFpsChanged();
  emit throttlingEnabledChanged();
  emit resolutionChanged();
  emit confidenceThresholdChanged();
  emit nmsThresholdChanged();
  emit gpuEnabledChanged();
  emit verboseLoggingChanged();
  emit darkModeChanged();
  emit displayOptionsChanged();
  emit lastCameraIdChanged();
  emit lastModelTypeChanged();
}

void SettingsManager::save() {
  CLIENT_INFO("Saving settings to storage...");

  // Camera settings
  settings_.setValue("camera/targetFps", target_fps_);
  settings_.setValue("camera/throttlingEnabled", throttling_enabled_);
  settings_.setValue("camera/resolutionWidth", resolution_width_);
  settings_.setValue("camera/resolutionHeight", resolution_height_);

  // Detection settings
  settings_.setValue("detection/confidenceThreshold", confidence_threshold_);
  settings_.setValue("detection/nmsThreshold", nms_threshold_);

  // Processing settings
  settings_.setValue("processing/gpuEnabled", gpu_enabled_);
  settings_.setValue("processing/verboseLogging", verbose_logging_);

  // Display settings
  settings_.setValue("display/darkMode", dark_mode_);
  settings_.setValue("display/showBoundingBoxes", show_bounding_boxes_);
  settings_.setValue("display/showConfidence", show_confidence_);
  settings_.setValue("display/showDistance", show_distance_);
  settings_.setValue("display/cameraPreviewVisible", camera_preview_visible_);

  // Last used settings
  settings_.setValue("lastUsed/cameraId", last_camera_id_);
  settings_.setValue("lastUsed/modelType", last_model_type_);

  settings_.sync();
  CLIENT_INFO("Settings saved successfully");
}

void SettingsManager::resetToDefaults() {
  CLIENT_INFO("Resetting settings to defaults...");

  settings_.clear();

  target_fps_ = 30;
  throttling_enabled_ = true;
  resolution_width_ = 640;
  resolution_height_ = 480;
  confidence_threshold_ = 0.5F;
  nms_threshold_ = 0.4F;
  gpu_enabled_ = false;
  verbose_logging_ = false;
  dark_mode_ = false;
  show_bounding_boxes_ = true;
  show_confidence_ = true;
  show_distance_ = true;
  camera_preview_visible_ = true;
  last_camera_id_ = "";
  last_model_type_ = 0;

  save();

  // Emit all changed signals
  emit targetFpsChanged();
  emit throttlingEnabledChanged();
  emit resolutionChanged();
  emit confidenceThresholdChanged();
  emit nmsThresholdChanged();
  emit gpuEnabledChanged();
  emit verboseLoggingChanged();
  emit darkModeChanged();
  emit displayOptionsChanged();
  emit lastCameraIdChanged();
  emit lastModelTypeChanged();

  CLIENT_INFO("Settings reset to defaults");
}

void SettingsManager::setTargetFps(int fps) noexcept {
  if (target_fps_ != fps) {
    target_fps_ = fps;
    save();
    emit targetFpsChanged();
  }
}

void SettingsManager::setThrottlingEnabled(bool enabled) noexcept {
  if (throttling_enabled_ != enabled) {
    throttling_enabled_ = enabled;
    save();
    emit throttlingEnabledChanged();
  }
}

void SettingsManager::setResolutionWidth(int width) noexcept {
  if (resolution_width_ != width) {
    resolution_width_ = width;
    save();
    emit resolutionChanged();
  }
}

void SettingsManager::setResolutionHeight(int height) noexcept {
  if (resolution_height_ != height) {
    resolution_height_ = height;
    save();
    emit resolutionChanged();
  }
}

void SettingsManager::setConfidenceThreshold(float threshold) noexcept {
  if (confidence_threshold_ != threshold) {
    confidence_threshold_ = threshold;
    save();
    emit confidenceThresholdChanged();
  }
}

void SettingsManager::setNmsThreshold(float threshold) noexcept {
  if (nms_threshold_ != threshold) {
    nms_threshold_ = threshold;
    save();
    emit nmsThresholdChanged();
  }
}

void SettingsManager::setGpuEnabled(bool enabled) noexcept {
  if (gpu_enabled_ != enabled) {
    gpu_enabled_ = enabled;
    save();
    emit gpuEnabledChanged();
  }
}

void SettingsManager::setVerboseLogging(bool enabled) noexcept {
  if (verbose_logging_ != enabled) {
    verbose_logging_ = enabled;
    save();
    emit verboseLoggingChanged();
  }
}

void SettingsManager::setDarkMode(bool enabled) noexcept {
  if (dark_mode_ != enabled) {
    dark_mode_ = enabled;
    save();
    emit darkModeChanged();
  }
}

void SettingsManager::setShowBoundingBoxes(bool show) noexcept {
  if (show_bounding_boxes_ != show) {
    show_bounding_boxes_ = show;
    save();
    emit displayOptionsChanged();
  }
}

void SettingsManager::setShowConfidence(bool show) noexcept {
  if (show_confidence_ != show) {
    show_confidence_ = show;
    save();
    emit displayOptionsChanged();
  }
}

void SettingsManager::setShowDistance(bool show) noexcept {
  if (show_distance_ != show) {
    show_distance_ = show;
    save();
    emit displayOptionsChanged();
  }
}

void SettingsManager::setCameraPreviewVisible(bool visible) noexcept {
  if (camera_preview_visible_ != visible) {
    camera_preview_visible_ = visible;
    save();
    emit displayOptionsChanged();
  }
}

void SettingsManager::setLastCameraId(const QString& id) noexcept {
  if (last_camera_id_ != id) {
    last_camera_id_ = id;
    save();
    emit lastCameraIdChanged();
  }
}

void SettingsManager::setLastModelType(int type) noexcept {
  if (last_model_type_ != type) {
    last_model_type_ = type;
    save();
    emit lastModelTypeChanged();
  }
}

}  // namespace client
