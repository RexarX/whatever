#pragma once

#include <client/pch.hpp>

#include <client/app/face_data.hpp>
#include <client/app/frame.hpp>
#include <client/app/model_config.hpp>
#include <client/core/logger.hpp>

#include <opencv2/dnn.hpp>
#include <opencv2/objdetect.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string_view>
#include <vector>

namespace client {

/**
 * @brief Error codes for face tracker operations.
 */
enum class FaceTrackerError : uint8_t {
  kModelNotFound,     ///< Model file not found.
  kModelLoadFailed,   ///< Failed to load the model.
  kConfigNotFound,    ///< Configuration file not found.
  kInvalidModel,      ///< Model is invalid or corrupted.
  kProcessingFailed,  ///< Frame processing failed.
  kNotInitialized     ///< Tracker not initialized.
};

/**
 * @brief Converts FaceTrackerError to a human-readable string.
 * @param error The error to convert.
 * @return A string view representing the error.
 */
[[nodiscard]] constexpr std::string_view FaceTrackerErrorToString(FaceTrackerError error) noexcept {
  switch (error) {
    case FaceTrackerError::kModelNotFound:
      return "Model file not found";
    case FaceTrackerError::kModelLoadFailed:
      return "Failed to load model";
    case FaceTrackerError::kConfigNotFound:
      return "Configuration file not found";
    case FaceTrackerError::kInvalidModel:
      return "Invalid or corrupted model";
    case FaceTrackerError::kProcessingFailed:
      return "Frame processing failed";
    case FaceTrackerError::kNotInitialized:
      return "Face tracker not initialized";
  }
  return "Unknown error";
}

/**
 * @brief Configuration for the face tracker.
 */
struct FaceTrackerConfig {
  std::filesystem::path model_path;   ///< Path to the neural network model.
  std::filesystem::path config_path;  ///< Path to the model configuration.
  float confidence_threshold = 0.5F;  ///< Minimum confidence for detection.
  float nms_threshold = 0.4F;         ///< Non-maximum suppression threshold.
  int input_width = 300;              ///< Model input width.
  int input_height = 300;             ///< Model input height.
  bool swap_rb = true;                ///< Swap Red and Blue channels.
  bool use_gpu = false;               ///< Use GPU acceleration if available.

  /**
   * @brief Creates FaceTrackerConfig from ModelConfig.
   * @param model_config The model configuration.
   * @return FaceTrackerConfig with model-specific settings.
   */
  [[nodiscard]] static FaceTrackerConfig FromModelConfig(const ModelConfig& model_config) noexcept {
    FaceTrackerConfig config;
    config.model_path = model_config.model_path;
    config.config_path = model_config.config_path;
    config.confidence_threshold = model_config.confidence_threshold;
    config.nms_threshold = model_config.nms_threshold;
    config.input_width = model_config.input_width;
    config.input_height = model_config.input_height;
    config.swap_rb = model_config.swap_rb;
    config.use_gpu = model_config.use_gpu;
    return config;
  }
};

/**
 * @brief DNN-based face detection and tracking.
 * @details Uses OpenCV's DNN module to load and run neural network models for face detection.
 * Supports various model formats including Caffe, TensorFlow, and ONNX.
 */
class FaceTracker {
public:
  /**
   * @brief Constructs an uninitialized face tracker.
   */
  FaceTracker() noexcept = default;

  /**
   * @brief Constructs and initializes a face tracker with configuration.
   * @param config Tracker configuration.
   */
  explicit FaceTracker(const FaceTrackerConfig& config);

  FaceTracker(const FaceTracker&) = delete;
  FaceTracker(FaceTracker&& other) noexcept;
  ~FaceTracker() noexcept = default;

  FaceTracker& operator=(const FaceTracker&) = delete;
  FaceTracker& operator=(FaceTracker&& other) noexcept;

  /**
   * @brief Initializes the face tracker with the given configuration.
   * @param config Tracker configuration.
   * @return Expected void on success, or FaceTrackerError on failure.
   */
  [[nodiscard]] auto Initialize(const FaceTrackerConfig& config) -> std::expected<void, FaceTrackerError>;

  /**
   * @brief Reinitializes the face tracker with a different model configuration.
   * @param model_config Model configuration to use.
   * @return Expected void on success, or FaceTrackerError on failure.
   */
  [[nodiscard]] auto Reinitialize(const ModelConfig& model_config) -> std::expected<void, FaceTrackerError> {
    return Initialize(FaceTrackerConfig::FromModelConfig(model_config));
  }

  /**
   * @brief Processes a frame and detects faces.
   * @param frame The input frame to process.
   * @return Expected FaceDetectionResult on success, or FaceTrackerError.
   */
  [[nodiscard]] auto Detect(const Frame& frame) -> std::expected<FaceDetectionResult, FaceTrackerError>;

  /**
   * @brief Updates the confidence threshold.
   * @param threshold New threshold value (0.0 - 1.0).
   */
  void ConfidenceThreshold(float threshold) noexcept { config_.confidence_threshold = threshold; }

  /**
   * @brief Sets the confidence threshold and updates the detector.
   * @param threshold New confidence threshold (0.0 - 1.0).
   */
  void SetConfidenceThreshold(float threshold) noexcept;

  /**
   * @brief Sets the NMS threshold and updates the detector.
   * @param threshold New NMS threshold (0.0 - 1.0).
   */
  void SetNmsThreshold(float threshold) noexcept;

  /**
   * @brief Checks if the tracker is initialized and ready.
   * @return True if initialized.
   */
  [[nodiscard]] bool Initialized() const noexcept { return initialized_; }

  /**
   * @brief Gets the current configuration.
   * @return Reference to the current configuration.
   */
  [[nodiscard]] const FaceTrackerConfig& Config() const noexcept { return config_; }

  /**
   * @brief Gets the confidence threshold.
   * @return Current confidence threshold.
   */
  [[nodiscard]] float ConfidenceThreshold() const noexcept { return config_.confidence_threshold; }

  /**
   * @brief Gets the total number of frames processed.
   * @return Frame count.
   */
  [[nodiscard]] uint64_t FramesProcessed() const noexcept { return frames_processed_; }

private:
  /**
   * @brief Creates a blob from the input frame for the network.
   * @param frame The input frame.
   * @return The blob matrix.
   */
  [[nodiscard]] cv::Mat CreateBlob(const Frame& frame) const;

  /**
   * @brief Parses YuNet detector output to extract face detections.
   * @param faces The FaceDetectorYN output matrix.
   * @param frame_width Original frame width.
   * @param frame_height Original frame height.
   * @return Vector of detected faces.
   */
  [[nodiscard]] auto ParseYuNetDetections(const cv::Mat& faces, int frame_width, int frame_height) const
      -> std::vector<FaceData>;

  /**
   * @brief Parses the network output to extract face detections.
   * @param output The network output matrix.
   * @param frame_width Original frame width.
   * @param frame_height Original frame height.
   * @return Vector of detected faces.
   */
  [[nodiscard]] auto ParseDetections(const cv::Mat& output, int frame_width, int frame_height) const
      -> std::vector<FaceData>;

  cv::dnn::Net net_;                            ///< The neural network (for SSD models).
  cv::Ptr<cv::FaceDetectorYN> yunet_detector_;  ///< YuNet face detector (for YuNet models).
  FaceTrackerConfig config_;                    ///< Current configuration.
  bool use_yunet_ = false;                      ///< Whether to use YuNet API instead of raw DNN.

  uint64_t frames_processed_ = 0;       ///< Counter for processed frames.
  mutable uint32_t next_track_id_ = 1;  ///< Next tracking ID to assign.
  bool initialized_ = false;            ///< Initialization status.
};

inline FaceTracker::FaceTracker(const FaceTrackerConfig& config) {
  const auto result = Initialize(config);
  if (!result) {
    CLIENT_ERROR("Failed to initialize FaceTracker: {}", FaceTrackerErrorToString(result.error()));
  }
}

inline FaceTracker::FaceTracker(FaceTracker&& other) noexcept
    : net_(std::move(other.net_)),
      config_(std::move(other.config_)),
      frames_processed_(other.frames_processed_),
      next_track_id_(other.next_track_id_),
      initialized_(other.initialized_) {
  other.initialized_ = false;
  other.frames_processed_ = 0;
  other.next_track_id_ = 1;
}

inline FaceTracker& FaceTracker::operator=(FaceTracker&& other) noexcept {
  if (this != &other) {
    net_ = std::move(other.net_);
    config_ = std::move(other.config_);
    frames_processed_ = other.frames_processed_;
    next_track_id_ = other.next_track_id_;
    initialized_ = other.initialized_;

    other.initialized_ = false;
    other.frames_processed_ = 0;
    other.next_track_id_ = 1;
  }
  return *this;
}

inline void FaceTracker::SetConfidenceThreshold(float threshold) noexcept {
  config_.confidence_threshold = threshold;
  if (use_yunet_ && !yunet_detector_.empty()) {
    yunet_detector_->setScoreThreshold(threshold);
    CLIENT_INFO("YuNet confidence threshold updated to: {:.2f}", threshold);
  } else {
    CLIENT_INFO("Confidence threshold updated to: {:.2f}", threshold);
  }
}

inline void FaceTracker::SetNmsThreshold(float threshold) noexcept {
  config_.nms_threshold = threshold;
  if (use_yunet_ && !yunet_detector_.empty()) {
    yunet_detector_->setNMSThreshold(threshold);
    CLIENT_INFO("YuNet NMS threshold updated to: {:.2f}", threshold);
  } else {
    CLIENT_INFO("NMS threshold updated to: {:.2f}", threshold);
  }
}

}  // namespace client
