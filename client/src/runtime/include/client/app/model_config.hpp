#pragma once

#include <client/pch.hpp>

#include <array>
#include <filesystem>
#include <string_view>

namespace client {

/**
 * @brief Enumeration of supported face detection models.
 */
enum class ModelType : uint8_t {
  kYuNetONNX,       ///< YuNet ONNX model (recommended).
  kResNet10Caffe,   ///< ResNet10 SSD Caffe model (high accuracy).
  kMobileNetCaffe,  ///< MobileNet SSD Caffe model (fast).
  kCustom           ///< Custom model configuration.
};

/**
 * @brief Converts ModelType to a human-readable string.
 * @param type The model type to convert.
 * @return A string view representing the model type.
 */
[[nodiscard]] constexpr std::string_view ModelTypeToString(ModelType type) noexcept {
  switch (type) {
    case ModelType::kYuNetONNX:
      return "YuNet ONNX";
    case ModelType::kResNet10Caffe:
      return "ResNet10 Caffe";
    case ModelType::kMobileNetCaffe:
      return "MobileNet Caffe";
    case ModelType::kCustom:
      return "Custom";
  }
  return "Unknown";
}

/**
 * @brief Configuration for a specific face detection model.
 * @details Contains all parameters needed to configure a face detection model,
 * including paths, thresholds, input dimensions, and backend settings.
 */
struct ModelConfig final {
  std::filesystem::path model_path;     ///< Path to the model file.
  std::filesystem::path config_path;    ///< Path to the config file (empty for ONNX).
  float confidence_threshold = 0.5F;    ///< Minimum confidence for detection (0.0-1.0).
  float nms_threshold = 0.4F;           ///< Non-maximum suppression threshold (0.0-1.0).
  int input_width = 320;                ///< Model input width in pixels.
  int input_height = 320;               ///< Model input height in pixels.
  bool swap_rb = true;                  ///< Swap Red and Blue channels.
  bool use_gpu = false;                 ///< Use GPU acceleration if available.
  ModelType type = ModelType::kCustom;  ///< Model type identifier.

  /**
   * @brief Creates a configuration for YuNet ONNX model.
   * @details YuNet is a modern, lightweight face detection model optimized for real-time applications.
   * It requires only a single ONNX file and provides good accuracy with fast inference times.
   * @param models_dir Base directory containing the models (default: "models").
   * @return ModelConfig configured for YuNet ONNX.
   */
  [[nodiscard]] static ModelConfig YuNetONNX(std::string_view models_dir = "models") noexcept;

  /**
   * @brief Creates a configuration for ResNet10 SSD Caffe model.
   * @details ResNet10 provides very high accuracy face detection with medium speed.
   * Requires both a .caffemodel and .prototxt file. Best for applications where accuracy is more important than speed.
   * @warning This model may have compatibility issues with certain OpenCV versions (shape mismatch in Eltwise layers).
   * If you encounter errors, use YuNet ONNX model instead which is more reliable.
   * @param models_dir Base directory containing the models (default: "models").
   * @return ModelConfig configured for ResNet10 Caffe.
   */
  [[nodiscard]] static ModelConfig ResNet10Caffe(std::string_view models_dir = "models") noexcept;

  /**
   * @brief Creates a configuration for MobileNet SSD Caffe model.
   * @details MobileNet is optimized for mobile and embedded devices,
   * providing very fast inference (~20-40ms on CPU) with good accuracy.
   * Requires both .caffemodel and .prototxt files.
   * @param models_dir Base directory containing the models (default: "models").
   * @return ModelConfig configured for MobileNet Caffe.
   */
  [[nodiscard]] static ModelConfig MobileNetCaffe(std::string_view models_dir = "models") noexcept;

  /**
   * @brief Creates a default model configuration based on model type.
   * @param type The model type to create configuration for.
   * @param models_dir Base directory containing the models (default: "models").
   * @return ModelConfig for the specified model type.
   */
  [[nodiscard]] static ModelConfig FromType(ModelType type, std::string_view models_dir = "models") noexcept;

  /**
   * @brief Gets the recommended default model configuration.
   * @details Returns YuNet ONNX configuration as it provides the best balance of speed,
   * accuracy, and ease of use for most applications.
   * @param models_dir Base directory containing the models (default: "models").
   * @return Default ModelConfig (YuNet ONNX).
   */
  [[nodiscard]] static ModelConfig Default(std::string_view models_dir = "models") noexcept {
    return YuNetONNX(models_dir);
  }

  /**
   * @brief Gets a list of all available predefined model configurations.
   * @param models_dir Base directory containing the models (default: "models").
   * @return Array of all available model configurations.
   */
  [[nodiscard]] static auto AllConfigs(std::string_view models_dir = "models") noexcept -> std::array<ModelConfig, 3> {
    return {YuNetONNX(models_dir), ResNet10Caffe(models_dir), MobileNetCaffe(models_dir)};
  }

  /**
   * @brief Validates that the model files exist.
   * @return True if all required model files exist.
   */
  [[nodiscard]] bool Validate() const noexcept;
};

inline ModelConfig ModelConfig::YuNetONNX(std::string_view models_dir) noexcept {
  ModelConfig config;
  config.model_path = std::filesystem::path(models_dir) / "face_detection_yunet_2023mar.onnx";
  config.config_path = "";  // ONNX doesn't need a separate config
  config.confidence_threshold = 0.5F;
  config.nms_threshold = 0.4F;
  config.input_width = 320;
  config.input_height = 320;
  config.swap_rb = true;
  config.use_gpu = false;
  config.type = ModelType::kYuNetONNX;
  return config;
}

inline ModelConfig ModelConfig::ResNet10Caffe(std::string_view models_dir) noexcept {
  ModelConfig config;
  config.model_path = std::filesystem::path(models_dir) / "res10_300x300_ssd_iter_140000.caffemodel";
  config.config_path = std::filesystem::path(models_dir) / "res10_300x300_ssd_deploy.prototxt";
  config.confidence_threshold = 0.5F;
  config.nms_threshold = 0.4F;
  config.input_width = 300;
  config.input_height = 300;
  config.swap_rb = true;
  config.use_gpu = false;
  config.type = ModelType::kResNet10Caffe;
  return config;
}

inline ModelConfig ModelConfig::MobileNetCaffe(std::string_view models_dir) noexcept {
  ModelConfig config;
  config.model_path = std::filesystem::path(models_dir) / "mobilenet_iter_73000.caffemodel";
  config.config_path = std::filesystem::path(models_dir) / "mobilenet_ssd_deploy.prototxt";
  config.confidence_threshold = 0.5F;
  config.nms_threshold = 0.4F;
  config.input_width = 300;
  config.input_height = 300;
  config.swap_rb = true;
  config.use_gpu = false;
  config.type = ModelType::kMobileNetCaffe;
  return config;
}

inline ModelConfig ModelConfig::FromType(ModelType type, std::string_view models_dir) noexcept {
  switch (type) {
    case ModelType::kYuNetONNX:
      return YuNetONNX(models_dir);
    case ModelType::kResNet10Caffe:
      return ResNet10Caffe(models_dir);
    case ModelType::kMobileNetCaffe:
      return MobileNetCaffe(models_dir);
    case ModelType::kCustom:
      return {};
  }
  return {};
}

inline bool ModelConfig::Validate() const noexcept {
  if (!std::filesystem::exists(model_path)) {
    return false;
  }

  // Check config path if specified (non-ONNX models)
  if (!config_path.empty() && !std::filesystem::exists(config_path)) {
    return false;
  }

  return true;
}

}  // namespace client
