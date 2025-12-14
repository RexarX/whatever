#include <doctest/doctest.h>

#include <client/app/model_config.hpp>

#include <filesystem>

TEST_SUITE("client::ModelConfig") {
  TEST_CASE("ModelType: ModelTypeToString returns correct strings") {
    CHECK_EQ(client::ModelTypeToString(client::ModelType::kYuNetONNX), "YuNet ONNX");
    CHECK_EQ(client::ModelTypeToString(client::ModelType::kResNet10Caffe), "ResNet10 Caffe");
    CHECK_EQ(client::ModelTypeToString(client::ModelType::kMobileNetCaffe), "MobileNet Caffe");
    CHECK_EQ(client::ModelTypeToString(client::ModelType::kCustom), "Custom");
  }

  TEST_CASE("ModelConfig: YuNetONNX creates correct configuration") {
    const auto config = client::ModelConfig::YuNetONNX();

    CHECK_EQ(config.model_path.filename().string(), "face_detection_yunet_2023mar.onnx");
    CHECK(config.config_path.empty());
    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.5f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.4f));
    CHECK_EQ(config.input_width, 320);
    CHECK_EQ(config.input_height, 320);
    CHECK(config.swap_rb);
    CHECK_FALSE(config.use_gpu);
    CHECK_EQ(config.type, client::ModelType::kYuNetONNX);
  }

  TEST_CASE("ModelConfig: ResNet10Caffe creates correct configuration") {
    const auto config = client::ModelConfig::ResNet10Caffe();

    CHECK_EQ(config.model_path.filename().string(), "res10_300x300_ssd_iter_140000.caffemodel");
    CHECK_EQ(config.config_path.filename().string(), "res10_300x300_ssd_deploy.prototxt");
    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.5f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.4f));
    CHECK_EQ(config.input_width, 300);
    CHECK_EQ(config.input_height, 300);
    CHECK(config.swap_rb);
    CHECK_FALSE(config.use_gpu);
    CHECK_EQ(config.type, client::ModelType::kResNet10Caffe);
  }

  TEST_CASE("ModelConfig: MobileNetCaffe creates correct configuration") {
    const auto config = client::ModelConfig::MobileNetCaffe();

    CHECK_EQ(config.model_path.filename().string(), "mobilenet_iter_73000.caffemodel");
    CHECK_EQ(config.config_path.filename().string(), "mobilenet_ssd_deploy.prototxt");
    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.5f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.4f));
    CHECK_EQ(config.input_width, 300);
    CHECK_EQ(config.input_height, 300);
    CHECK(config.swap_rb);
    CHECK_FALSE(config.use_gpu);
    CHECK_EQ(config.type, client::ModelType::kMobileNetCaffe);
  }

  TEST_CASE("ModelConfig: YuNetONNX with custom models directory") {
    const auto config = client::ModelConfig::YuNetONNX("/custom/path");

    CHECK(config.model_path.string().find("/custom/path") != std::string::npos);
    CHECK_EQ(config.model_path.filename().string(), "face_detection_yunet_2023mar.onnx");
  }

  TEST_CASE("ModelConfig: Default returns YuNet ONNX") {
    const auto config = client::ModelConfig::Default();

    CHECK_EQ(config.type, client::ModelType::kYuNetONNX);
    CHECK_EQ(config.model_path.filename().string(), "face_detection_yunet_2023mar.onnx");
  }

  TEST_CASE("ModelConfig: FromType creates correct configuration") {
    SUBCASE("YuNet ONNX") {
      const auto config = client::ModelConfig::FromType(client::ModelType::kYuNetONNX);
      CHECK_EQ(config.type, client::ModelType::kYuNetONNX);
      CHECK_EQ(config.model_path.filename().string(), "face_detection_yunet_2023mar.onnx");
    }

    SUBCASE("ResNet10 Caffe") {
      const auto config = client::ModelConfig::FromType(client::ModelType::kResNet10Caffe);
      CHECK_EQ(config.type, client::ModelType::kResNet10Caffe);
      CHECK_EQ(config.model_path.filename().string(), "res10_300x300_ssd_iter_140000.caffemodel");
    }

    SUBCASE("MobileNet Caffe") {
      const auto config = client::ModelConfig::FromType(client::ModelType::kMobileNetCaffe);
      CHECK_EQ(config.type, client::ModelType::kMobileNetCaffe);
      CHECK_EQ(config.model_path.filename().string(), "mobilenet_iter_73000.caffemodel");
    }

    SUBCASE("Custom") {
      const auto config = client::ModelConfig::FromType(client::ModelType::kCustom);
      CHECK_EQ(config.type, client::ModelType::kCustom);
    }
  }

  TEST_CASE("ModelConfig: AllConfigs returns all model configurations") {
    const auto configs = client::ModelConfig::AllConfigs();

    CHECK_EQ(configs.size(), 3);
    CHECK_EQ(configs[0].type, client::ModelType::kYuNetONNX);
    CHECK_EQ(configs[1].type, client::ModelType::kResNet10Caffe);
    CHECK_EQ(configs[2].type, client::ModelType::kMobileNetCaffe);
  }

  TEST_CASE("ModelConfig: Validate returns false for non-existent model") {
    client::ModelConfig config;
    config.model_path = "/non/existent/model.onnx";
    config.config_path = "";

    CHECK_FALSE(config.Validate());
  }

  TEST_CASE("ModelConfig: Validate returns false for non-existent config") {
    client::ModelConfig config;
    config.model_path = "/dev/null";  // Exists on Unix systems
    config.config_path = "/non/existent/config.prototxt";

    CHECK_FALSE(config.Validate());
  }

  TEST_CASE("ModelConfig: Validate returns true when only model exists and config is empty") {
    client::ModelConfig config;
    config.model_path = "/dev/null";  // Exists on Unix systems
    config.config_path = "";          // Empty config path

    CHECK(config.Validate());
  }

  TEST_CASE("ModelConfig: Custom configuration can be created") {
    client::ModelConfig config;
    config.model_path = "custom_model.onnx";
    config.config_path = "";
    config.confidence_threshold = 0.75f;
    config.nms_threshold = 0.35f;
    config.input_width = 416;
    config.input_height = 416;
    config.swap_rb = false;
    config.use_gpu = true;
    config.type = client::ModelType::kCustom;

    CHECK_EQ(config.model_path, "custom_model.onnx");
    CHECK(config.config_path.empty());
    CHECK_EQ(config.confidence_threshold, doctest::Approx(0.75f));
    CHECK_EQ(config.nms_threshold, doctest::Approx(0.35f));
    CHECK_EQ(config.input_width, 416);
    CHECK_EQ(config.input_height, 416);
    CHECK_FALSE(config.swap_rb);
    CHECK(config.use_gpu);
    CHECK_EQ(config.type, client::ModelType::kCustom);
  }

  TEST_CASE("ModelType: All enum values are distinct") {
    CHECK_NE(client::ModelType::kYuNetONNX, client::ModelType::kResNet10Caffe);
    CHECK_NE(client::ModelType::kResNet10Caffe, client::ModelType::kMobileNetCaffe);
    CHECK_NE(client::ModelType::kMobileNetCaffe, client::ModelType::kCustom);
    CHECK_NE(client::ModelType::kYuNetONNX, client::ModelType::kCustom);
  }

  TEST_CASE("ModelTypeToString: Constexpr function") {
    constexpr auto model_str = client::ModelTypeToString(client::ModelType::kYuNetONNX);
    CHECK_EQ(model_str, "YuNet ONNX");
  }
}
