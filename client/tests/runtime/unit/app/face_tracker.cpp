#include <doctest/doctest.h>

#include <client/app/face_tracker.hpp>

#include <filesystem>

TEST_SUITE("client::FaceTracker") {
  TEST_CASE("FaceTrackerError: FaceTrackerErrorToString returns correct strings") {
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kModelNotFound), "Model file not found");
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kModelLoadFailed), "Failed to load model");
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kConfigNotFound),
             "Configuration file not found");
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kInvalidModel), "Invalid or corrupted model");
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kProcessingFailed), "Frame processing failed");
    CHECK_EQ(client::FaceTrackerErrorToString(client::FaceTrackerError::kNotInitialized),
             "Face tracker not initialized");
  }

  TEST_CASE("FaceTrackerConfig: Default values") {
    client::FaceTrackerConfig config;

    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.5f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.4f));
    CHECK_EQ(config.input_width, 300);
    CHECK_EQ(config.input_height, 300);
    CHECK(config.swap_rb);
    CHECK_FALSE(config.use_gpu);
    CHECK(config.model_path.empty());
    CHECK(config.config_path.empty());
  }

  TEST_CASE("FaceTracker: Default construction is not initialized") {
    client::FaceTracker tracker;

    CHECK_FALSE(tracker.Initialized());
    CHECK_EQ(tracker.FramesProcessed(), 0u);
  }

  TEST_CASE("FaceTracker: Initialize with non-existent model returns error") {
    client::FaceTracker tracker;
    client::FaceTrackerConfig config;
    config.model_path = "/non/existent/path/model.onnx";

    auto result = tracker.Initialize(config);

    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::FaceTrackerError::kModelNotFound);
    CHECK_FALSE(tracker.Initialized());
  }

  TEST_CASE("FaceTracker: Initialize with non-existent config returns error") {
    // Create a temporary model file path (won't actually be a valid model)
    client::FaceTracker tracker;
    client::FaceTrackerConfig config;

    // Use a path that exists on most systems
    config.model_path = "/dev/null";  // Exists but not a valid model
    config.config_path = "/non/existent/config.prototxt";

    auto result = tracker.Initialize(config);

    // Should fail because config file doesn't exist
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::FaceTrackerError::kConfigNotFound);
  }

  TEST_CASE("FaceTracker: Detect without initialization returns error") {
    client::FaceTracker tracker;
    client::Frame frame(100, 100, CV_8UC3);

    auto result = tracker.Detect(frame);

    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::FaceTrackerError::kNotInitialized);
  }

  TEST_CASE("FaceTracker: Config getter returns current config") {
    client::FaceTracker tracker;
    client::FaceTrackerConfig config;
    config.confidence_threshold = 0.75f;
    config.input_width = 320;
    config.input_height = 320;
    config.model_path = "/some/path.onnx";

    // Even if initialize fails, config should be stored
    [[maybe_unused]] auto result = tracker.Initialize(config);

    const auto& stored_config = tracker.Config();
    CHECK_EQ(stored_config.confidence_threshold, doctest::Approx(0.75f));
    CHECK_EQ(stored_config.input_width, 320);
    CHECK_EQ(stored_config.input_height, 320);
  }

  TEST_CASE("FaceTracker: ConfidenceThreshold getter and setter") {
    client::FaceTracker tracker;

    tracker.ConfidenceThreshold(0.8f);

    CHECK_EQ(tracker.ConfidenceThreshold(), doctest::Approx(0.8f));
  }

  TEST_CASE("FaceTracker: FramesProcessed starts at zero") {
    client::FaceTracker tracker;

    CHECK_EQ(tracker.FramesProcessed(), 0u);
  }

  TEST_CASE("FaceTracker: Move construction") {
    client::FaceTracker original;
    original.ConfidenceThreshold(0.6f);

    client::FaceTracker moved(std::move(original));

    CHECK_EQ(moved.ConfidenceThreshold(), doctest::Approx(0.6f));
    CHECK_FALSE(original.Initialized());
  }

  TEST_CASE("FaceTracker: Move assignment") {
    client::FaceTracker original;
    original.ConfidenceThreshold(0.7f);

    client::FaceTracker target;
    target = std::move(original);

    CHECK_EQ(target.ConfidenceThreshold(), doctest::Approx(0.7f));
    CHECK_FALSE(original.Initialized());
  }

  TEST_CASE("FaceTracker: Detect with empty frame returns error") {
    client::FaceTracker tracker;

    // Even if not initialized, empty frame should be checked
    // But we'll get not initialized error first
    client::Frame empty_frame;
    auto result = tracker.Detect(empty_frame);

    CHECK_FALSE(result.has_value());
    // Either not initialized or processing failed is acceptable
    CHECK((result.error() == client::FaceTrackerError::kNotInitialized ||
           result.error() == client::FaceTrackerError::kProcessingFailed));
  }

  TEST_CASE("FaceTrackerConfig: Custom configuration") {
    client::FaceTrackerConfig config;
    config.model_path = "custom_model.onnx";
    config.config_path = "custom_config.pbtxt";
    config.confidence_threshold = 0.9f;
    config.nms_threshold = 0.3f;
    config.input_width = 416;
    config.input_height = 416;
    config.swap_rb = false;
    config.use_gpu = true;

    CHECK_EQ(config.model_path, "custom_model.onnx");
    CHECK_EQ(config.config_path, "custom_config.pbtxt");
    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.9f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.3f));
    CHECK_EQ(config.input_width, 416);
    CHECK_EQ(config.input_height, 416);
    CHECK_FALSE(config.swap_rb);
    CHECK(config.use_gpu);
  }

  TEST_CASE("FaceTrackerError: Error codes are distinct") {
    CHECK_NE(client::FaceTrackerError::kModelNotFound, client::FaceTrackerError::kModelLoadFailed);
    CHECK_NE(client::FaceTrackerError::kModelLoadFailed, client::FaceTrackerError::kConfigNotFound);
    CHECK_NE(client::FaceTrackerError::kConfigNotFound, client::FaceTrackerError::kInvalidModel);
    CHECK_NE(client::FaceTrackerError::kInvalidModel, client::FaceTrackerError::kProcessingFailed);
    CHECK_NE(client::FaceTrackerError::kProcessingFailed, client::FaceTrackerError::kNotInitialized);
  }

  TEST_CASE("FaceTrackerErrorToString: Constexpr function") {
    constexpr auto error_str = client::FaceTrackerErrorToString(client::FaceTrackerError::kNotInitialized);

    CHECK_EQ(error_str, "Face tracker not initialized");
  }

  TEST_CASE("FaceTrackerConfig: FromModelConfig creates correct configuration") {
    const auto model_config = client::ModelConfig::YuNetONNX("models");
    const auto tracker_config = client::FaceTrackerConfig::FromModelConfig(model_config);

    CHECK_EQ(tracker_config.model_path, model_config.model_path);
    CHECK_EQ(tracker_config.config_path, model_config.config_path);
    CHECK_EQ(tracker_config.confidence_threshold, doctest::Approx(model_config.confidence_threshold));
    CHECK_EQ(tracker_config.nms_threshold, doctest::Approx(model_config.nms_threshold));
    CHECK_EQ(tracker_config.input_width, model_config.input_width);
    CHECK_EQ(tracker_config.input_height, model_config.input_height);
    CHECK_EQ(tracker_config.swap_rb, model_config.swap_rb);
    CHECK_EQ(tracker_config.use_gpu, model_config.use_gpu);
  }

  TEST_CASE("FaceTrackerConfig: FromModelConfig with ResNet10") {
    const auto model_config = client::ModelConfig::ResNet10Caffe("models");
    const auto tracker_config = client::FaceTrackerConfig::FromModelConfig(model_config);

    CHECK_EQ(tracker_config.input_width, 300);
    CHECK_EQ(tracker_config.input_height, 300);
    CHECK_FALSE(tracker_config.config_path.empty());
  }

  TEST_CASE("FaceTracker: Reinitialize without initialization returns error") {
    client::FaceTracker tracker;
    const auto model_config = client::ModelConfig::YuNetONNX();

    // Reinitialize on uninitialized tracker (should work, just calls Initialize)
    auto result = tracker.Reinitialize(model_config);

    // Model file may not exist, so failure is acceptable
    if (!result) {
      CHECK((result.error() == client::FaceTrackerError::kModelNotFound ||
             result.error() == client::FaceTrackerError::kModelLoadFailed));
    }
  }

  TEST_CASE("FaceTracker: Reinitialize with invalid model returns error") {
    client::FaceTracker tracker;
    client::ModelConfig model_config;
    model_config.model_path = "/non/existent/model.onnx";

    auto result = tracker.Reinitialize(model_config);

    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::FaceTrackerError::kModelNotFound);
  }
}
