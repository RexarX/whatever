#include <client/app/face_tracker.hpp>

#include <client/core/assert.hpp>
#include <client/core/logger.hpp>

#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <utility>

#include <opencv2/dnn.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect.hpp>

namespace client {

auto FaceTracker::Initialize(const FaceTrackerConfig& config) -> std::expected<void, FaceTrackerError> {
  config_ = config;

  // Check if model file exists
  if (!std::filesystem::exists(config_.model_path)) {
    CLIENT_ERROR("Model file not found: {}", config_.model_path.string());
    return std::unexpected(FaceTrackerError::kModelNotFound);
  }

  // Check if config file exists (if specified)
  if (!config_.config_path.empty() && !std::filesystem::exists(config_.config_path)) {
    CLIENT_ERROR("Config file not found: {}", config_.config_path.string());
    return std::unexpected(FaceTrackerError::kConfigNotFound);
  }

  try {
    // Determine if this is a YuNet ONNX model (no config file and .onnx extension)
    use_yunet_ = config_.config_path.empty() && config_.model_path.extension() == ".onnx";

    if (use_yunet_) {
      // Use FaceDetectorYN for YuNet models
      CLIENT_INFO("Loading YuNet model using FaceDetectorYN API");

      yunet_detector_ = cv::FaceDetectorYN::create(config_.model_path.string(),
                                                   "",  // No config needed for ONNX
                                                   cv::Size(config_.input_width, config_.input_height),
                                                   config_.confidence_threshold, config_.nms_threshold);

      if (yunet_detector_.empty()) {
        CLIENT_ERROR("Failed to create FaceDetectorYN");
        return std::unexpected(FaceTrackerError::kModelLoadFailed);
      }

      CLIENT_INFO("FaceDetectorYN initialized successfully");
    } else {
      // Use regular DNN for Caffe models
      CLIENT_INFO("Loading model using OpenCV DNN");

      if (config_.config_path.empty()) {
        net_ = cv::dnn::readNet(config_.model_path.string());
      } else {
        net_ = cv::dnn::readNet(config_.model_path.string(), config_.config_path.string());
      }

      if (net_.empty()) {
        CLIENT_ERROR("Failed to load neural network model");
        return std::unexpected(FaceTrackerError::kModelLoadFailed);
      }

      // Configure backend and target
      if (config_.use_gpu) {
        try {
          net_.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
          net_.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
          CLIENT_INFO("FaceTracker using CUDA backend");
        } catch (const cv::Exception& e) {
          CLIENT_WARN("Failed to set CUDA backend, falling back to CPU: {}", e.what());
          net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
          net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        }
      } else {
        net_.setPreferableBackend(cv::dnn::DNN_BACKEND_DEFAULT);
        net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        CLIENT_INFO("FaceTracker using CPU backend");
      }

      // Test the network with a dummy forward pass
      try {
        const int dims[] = {1, 3, config_.input_height, config_.input_width};
        cv::Mat dummy_blob = cv::Mat::zeros(4, dims, CV_32F);
        net_.setInput(dummy_blob);
        cv::Mat test_output = net_.forward();
        if (test_output.empty()) {
          CLIENT_ERROR("Model test forward pass produced empty output");
          net_ = cv::dnn::Net();  // Clear invalid network
          return std::unexpected(FaceTrackerError::kModelLoadFailed);
        }
        CLIENT_INFO("Model test forward pass successful, output dims: {}", test_output.dims);
      } catch (const cv::Exception& e) {
        CLIENT_ERROR("Model test forward pass failed: {}", e.what());
        CLIENT_ERROR("This model may be incompatible with your OpenCV version or have corrupted layers");
        CLIENT_ERROR("Hint: The prototxt file may have duplicate blob names or incompatible layer definitions");
        net_ = cv::dnn::Net();  // Clear invalid network
        return std::unexpected(FaceTrackerError::kInvalidModel);
      }
    }

    initialized_ = true;
    CLIENT_INFO("FaceTracker initialized with model: {}", config_.model_path.filename().string());

    return {};
  } catch (const cv::Exception& e) {
    CLIENT_ERROR("OpenCV exception during model loading: {}", e.what());
    CLIENT_ERROR("Model file: {}", config_.model_path.string());
    if (!config_.config_path.empty()) {
      CLIENT_ERROR("Config file: {}", config_.config_path.string());
    }
    net_ = cv::dnn::Net();    // Clear invalid network
    yunet_detector_.reset();  // Clear detector
    return std::unexpected(FaceTrackerError::kModelLoadFailed);
  }
}

auto FaceTracker::Detect(const Frame& frame) -> std::expected<FaceDetectionResult, FaceTrackerError> {
  if (!initialized_) {
    return std::unexpected(FaceTrackerError::kNotInitialized);
  }

  if (frame.Empty()) {
    return std::unexpected(FaceTrackerError::kProcessingFailed);
  }

  // Additional safety check for invalid network state
  if (!use_yunet_ && net_.empty()) {
    CLIENT_ERROR("Neural network is in invalid state (empty)");
    return std::unexpected(FaceTrackerError::kNotInitialized);
  }

  if (use_yunet_ && yunet_detector_.empty()) {
    CLIENT_ERROR("YuNet detector is in invalid state (empty)");
    return std::unexpected(FaceTrackerError::kNotInitialized);
  }

  FaceDetectionResult result;
  result.frame_id = frames_processed_;

  auto start_time = std::chrono::high_resolution_clock::now();

  try {
    if (use_yunet_) {
      // Use YuNet detector
      yunet_detector_->setInputSize(cv::Size(frame.Width(), frame.Height()));

      cv::Mat faces;
      yunet_detector_->detect(frame.Mat(), faces);

      if (!faces.empty()) {
        result.faces = ParseYuNetDetections(faces, frame.Width(), frame.Height());
      }
    } else {
      // Use regular DNN
      cv::Mat blob = CreateBlob(frame);

      if (blob.empty()) {
        CLIENT_ERROR("Failed to create blob from frame");
        return std::unexpected(FaceTrackerError::kProcessingFailed);
      }

      net_.setInput(blob);
      cv::Mat output = net_.forward();

      if (output.empty()) {
        CLIENT_ERROR("Network forward pass produced empty output");
        return std::unexpected(FaceTrackerError::kProcessingFailed);
      }

      result.faces = ParseDetections(output, frame.Width(), frame.Height());
    }

    // Calculate relative distance for all detected faces
    for (auto& face : result.faces) {
      face.relative_distance = face.CalculateRelativeDistance(frame.Width(), frame.Height());
    }

    // Sort faces by priority (closest and most confident first)
    result.SortByPriority();

    auto end_time = std::chrono::high_resolution_clock::now();
    result.processing_time_ms = std::chrono::duration<float, std::milli>(end_time - start_time).count();

    ++frames_processed_;

    return result;
  } catch (const cv::Exception& e) {
    CLIENT_ERROR("OpenCV exception during face detection: {}", e.what());
    return std::unexpected(FaceTrackerError::kProcessingFailed);
  }
}

cv::Mat FaceTracker::CreateBlob(const Frame& frame) const {
  // Create a 4D blob from the image
  // The blob has dimensions [batch_size, channels, height, width]

  // Use appropriate mean values based on model format
  // YuNet ONNX: [0, 0, 0] (no mean subtraction)
  // SSD/Caffe models: [104.0, 177.0, 123.0]
  cv::Scalar mean_values(0.0, 0.0, 0.0);

  // Detect model type based on config path (empty = ONNX, non-empty = Caffe)
  if (!config_.config_path.empty()) {
    // Caffe models need mean subtraction
    mean_values = cv::Scalar(104.0, 177.0, 123.0);
  }

  return cv::dnn::blobFromImage(frame.Mat(),
                                1.0,  // Scale factor
                                cv::Size(config_.input_width, config_.input_height), mean_values, config_.swap_rb,
                                false  // Don't crop
  );
}

auto FaceTracker::ParseYuNetDetections(const cv::Mat& faces, int frame_width, int frame_height) const
    -> std::vector<FaceData> {
  // FaceDetectorYN returns detections in format:
  // [x, y, w, h, x_re, y_re, x_le, y_le, x_nt, y_nt, x_rcm, y_rcm, x_lcm, y_lcm, score]
  // Shape: [N, 15] where N is number of detections
  // Coordinates are already in pixel coordinates relative to input image size

  std::vector<FaceData> face_list;

  if (faces.empty() || faces.rows == 0) {
    return face_list;
  }

  // Log output shape for debugging (only once)
  static bool shape_logged = false;
  if (!shape_logged) {
    CLIENT_INFO("YuNet output shape: rows={}, cols={}", faces.rows, faces.cols);
    shape_logged = true;
  }

  for (int i = 0; i < faces.rows; ++i) {
    // Get detection data
    const float x = faces.at<float>(i, 0);
    const float y = faces.at<float>(i, 1);
    const float w = faces.at<float>(i, 2);
    const float h = faces.at<float>(i, 3);
    const float confidence = faces.at<float>(i, 14);

    // Validate confidence
    if (confidence < config_.confidence_threshold) {
      continue;
    }

    // Validate dimensions
    if (w <= 0.0F || h <= 0.0F) {
      continue;
    }

    FaceData face;
    face.bounding_box.x = x;
    face.bounding_box.y = y;
    face.bounding_box.width = w;
    face.bounding_box.height = h;
    face.confidence = confidence;
    face.track_id = next_track_id_++;

    // Clamp bounding box to frame bounds
    if (face.bounding_box.x < 0.0F) {
      face.bounding_box.width += face.bounding_box.x;
      face.bounding_box.x = 0.0F;
    }
    if (face.bounding_box.y < 0.0F) {
      face.bounding_box.height += face.bounding_box.y;
      face.bounding_box.y = 0.0F;
    }
    if (face.bounding_box.x + face.bounding_box.width > static_cast<float>(frame_width)) {
      face.bounding_box.width = static_cast<float>(frame_width) - face.bounding_box.x;
    }
    if (face.bounding_box.y + face.bounding_box.height > static_cast<float>(frame_height)) {
      face.bounding_box.height = static_cast<float>(frame_height) - face.bounding_box.y;
    }

    // Validate final bounding box
    if (face.bounding_box.width > 0.0F && face.bounding_box.height > 0.0F) {
      face_list.push_back(face);
    }
  }

  return face_list;
}

auto FaceTracker::ParseDetections(const cv::Mat& output, int frame_width, int frame_height) const
    -> std::vector<FaceData> {
  // SSD-style detectors output: [1, 1, N, 7]
  // [batch_id, class_id, confidence, x1, y1, x2, y2]
  // Coordinates are normalized (0-1)

  std::vector<FaceData> faces;

  if (output.empty()) {
    CLIENT_WARN("Empty output from network");
    return faces;
  }

  // Log output shape for debugging (only once)
  static bool shape_logged = false;
  if (!shape_logged) {
    std::string shape_str = "[";
    shape_str.reserve(2 + static_cast<size_t>(output.size().area()));
    for (int i = 0; i < output.dims; ++i) {
      if (i > 0) {
        shape_str += ", ";
      }
      shape_str += std::to_string(output.size[i]);
    }
    shape_str += "]";
    CLIENT_INFO("SSD Model output shape: dims={}, size={}, type={}", output.dims, shape_str, output.type());
    shape_logged = true;
  }

  // Handle different output formats
  cv::Mat detections;

  if (output.dims == 4) {
    // Standard SSD output [1, 1, N, 7]
    const int num_detections = output.size[2];
    const int values_per_detection = output.size[3];

    // Reshape to 2D: [N, 7]
    detections = output.reshape(1, num_detections);

    if (detections.cols != values_per_detection) {
      // Reshape didn't work as expected, try manual extraction
      detections = cv::Mat(num_detections, values_per_detection, CV_32F);

      for (int i = 0; i < num_detections; ++i) {
        for (int j = 0; j < values_per_detection; ++j) {
          // Access 4D tensor: [0, 0, i, j]
          const float* data = output.ptr<float>(0, 0);
          detections.at<float>(i, j) = data[i * values_per_detection + j];
        }
      }
    }
  } else if (output.dims == 2) {
    // Already in [N, 7] format
    detections = output;
  } else if (output.dims == 3) {
    // Sometimes output is [1, N, 7]
    const int num_detections = output.size[1];
    const int values_per_detection = output.size[2];

    detections = cv::Mat(num_detections, values_per_detection, CV_32F);

    for (int i = 0; i < num_detections; ++i) {
      for (int j = 0; j < values_per_detection; ++j) {
        const float* data = output.ptr<float>(0);
        detections.at<float>(i, j) = data[i * values_per_detection + j];
      }
    }
  } else {
    std::string shape_str = "[";
    for (int i = 0; i < output.dims; ++i) {
      if (i > 0)
        shape_str.append(", ");
      shape_str += std::to_string(output.size[i]);
    }
    shape_str.append("]");
    CLIENT_WARN("Unexpected output tensor format: dims={}, shape={}", output.dims, shape_str);
    return faces;
  }

  // Validate detections matrix
  if (detections.empty() || detections.cols < 7) {
    CLIENT_WARN("Invalid detections matrix: rows={}, cols={}", detections.rows, detections.cols);
    return faces;
  }

  // Parse SSD detections
  for (int i = 0; i < detections.rows; ++i) {
    const float confidence = detections.at<float>(i, 2);

    if (confidence < config_.confidence_threshold) {
      continue;
    }

    // Get bounding box coordinates (normalized 0-1)
    const float x1 = detections.at<float>(i, 3);
    const float y1 = detections.at<float>(i, 4);
    const float x2 = detections.at<float>(i, 5);
    const float y2 = detections.at<float>(i, 6);

    // Validate normalized coordinates
    if (x1 < 0.0F || x1 > 1.0F || y1 < 0.0F || y1 > 1.0F || x2 < 0.0F || x2 > 1.0F || y2 < 0.0F || y2 > 1.0F ||
        x1 >= x2 || y1 >= y2) {
      continue;
    }

    // Convert to pixel coordinates
    FaceData face;
    face.bounding_box.x = x1 * static_cast<float>(frame_width);
    face.bounding_box.y = y1 * static_cast<float>(frame_height);
    face.bounding_box.width = (x2 - x1) * static_cast<float>(frame_width);
    face.bounding_box.height = (y2 - y1) * static_cast<float>(frame_height);
    face.confidence = confidence;
    face.track_id = next_track_id_++;

    // Clamp bounding box to frame bounds
    if (face.bounding_box.x < 0.0F) {
      face.bounding_box.width += face.bounding_box.x;
      face.bounding_box.x = 0.0F;
    }
    if (face.bounding_box.y < 0.0F) {
      face.bounding_box.height += face.bounding_box.y;
      face.bounding_box.y = 0.0F;
    }
    if (face.bounding_box.x + face.bounding_box.width > static_cast<float>(frame_width)) {
      face.bounding_box.width = static_cast<float>(frame_width) - face.bounding_box.x;
    }
    if (face.bounding_box.y + face.bounding_box.height > static_cast<float>(frame_height)) {
      face.bounding_box.height = static_cast<float>(frame_height) - face.bounding_box.y;
    }

    // Validate final bounding box
    if (face.bounding_box.width > 0.0F && face.bounding_box.height > 0.0F) {
      faces.push_back(face);
    }
  }

  // Apply Non-Maximum Suppression if we have multiple detections
  if (faces.size() > 1 && config_.nms_threshold > 0.0F) {
    std::vector<cv::Rect> boxes;
    std::vector<float> scores;
    boxes.reserve(faces.size());
    scores.reserve(faces.size());

    for (const auto& face : faces) {
      boxes.emplace_back(static_cast<int>(face.bounding_box.x), static_cast<int>(face.bounding_box.y),
                         static_cast<int>(face.bounding_box.width), static_cast<int>(face.bounding_box.height));
      scores.push_back(face.confidence);
    }

    std::vector<int> indices;
    cv::dnn::NMSBoxes(boxes, scores, config_.confidence_threshold, config_.nms_threshold, indices);

    std::vector<FaceData> nms_faces;
    nms_faces.reserve(indices.size());
    for (int idx : indices) {
      nms_faces.push_back(faces[static_cast<size_t>(idx)]);
    }
    faces = std::move(nms_faces);
  }

  return faces;
}

}  // namespace client
