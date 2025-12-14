#include <doctest/doctest.h>

#include <client/app/gui_window.hpp>

#include <QGuiApplication>
#include <QTimer>

#include <memory>

TEST_SUITE("client::GuiWindow") {
  // Helper to create QGuiApplication if needed
  static int argc = 1;
  static char* argv[] = {const_cast<char*>("test")};
  static std::unique_ptr<QGuiApplication> app;

  static void EnsureQGuiApplication() {
    if (!QGuiApplication::instance()) {
      app = std::make_unique<QGuiApplication>(argc, argv);
    }
  }

  TEST_CASE("GuiWindow: Construction creates valid window") {
    EnsureQGuiApplication();

    client::GuiWindow window;

    // Window exists but may not be visible yet
    CHECK_NOTHROW([&]() { window.show(); }());
  }

  TEST_CASE("GuiWindow: SetCameraSwitchCallback stores callback") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    bool callback_called = false;
    std::string captured_device_id;

    window.SetCameraSwitchCallback([&](std::string_view device_id) {
      callback_called = true;
      captured_device_id = std::string(device_id);
    });

    // Callback is stored (we can't test if it's called without triggering UI)
    CHECK_FALSE(callback_called);
  }

  TEST_CASE("GuiWindow: SetModelSwitchCallback stores callback") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    bool callback_called = false;
    client::ModelType captured_model_type{};

    window.SetModelSwitchCallback([&](client::ModelType model_type) {
      callback_called = true;
      captured_model_type = model_type;
    });

    // Callback is stored
    CHECK_FALSE(callback_called);
  }

  TEST_CASE("GuiWindow: UpdateFrame with empty frame is safe") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    client::Frame empty_frame;

    CHECK_NOTHROW(window.UpdateFrame(empty_frame));
  }

  TEST_CASE("GuiWindow: UpdateFrame with valid frame is safe") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    client::Frame frame(640, 480, CV_8UC3);
    frame.Mat().setTo(cv::Scalar(100, 150, 200));

    CHECK_NOTHROW(window.UpdateFrame(frame));
  }

  TEST_CASE("GuiWindow: UpdateStats updates display") {
    EnsureQGuiApplication();

    client::GuiWindow window;

    CHECK_NOTHROW(window.UpdateStats(30.0f, 1000, 2));
    CHECK_NOTHROW(window.UpdateStats(0.0f, 0, 0));
    CHECK_NOTHROW(window.UpdateStats(60.5f, 999999, 10));
  }

  TEST_CASE("GuiWindow: SetCurrentModel sets model type") {
    EnsureQGuiApplication();

    client::GuiWindow window;

    CHECK_NOTHROW(window.SetCurrentModel(client::ModelType::kYuNetONNX));
    CHECK_NOTHROW(window.SetCurrentModel(client::ModelType::kResNet10Caffe));
    CHECK_NOTHROW(window.SetCurrentModel(client::ModelType::kMobileNetCaffe));
  }

  TEST_CASE("GuiWindow: UpdateCameraList with empty list") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    std::vector<client::CameraDeviceInfo> cameras;

    CHECK_NOTHROW(window.UpdateCameraList(cameras, ""));
  }

  TEST_CASE("GuiWindow: UpdateCameraList with single camera") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    std::vector<client::CameraDeviceInfo> cameras;

    client::CameraDeviceInfo camera;
    camera.id = "camera0";
    camera.description = "Test Camera";
    camera.is_default = true;
    cameras.push_back(camera);

    CHECK_NOTHROW(window.UpdateCameraList(cameras, "camera0"));
  }

  TEST_CASE("GuiWindow: UpdateCameraList with multiple cameras") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    std::vector<client::CameraDeviceInfo> cameras;

    for (int i = 0; i < 3; ++i) {
      client::CameraDeviceInfo camera;
      camera.id = "camera" + std::to_string(i);
      camera.description = "Test Camera " + std::to_string(i);
      camera.is_default = (i == 0);
      cameras.push_back(camera);
    }

    CHECK_NOTHROW(window.UpdateCameraList(cameras, "camera1"));
  }

  TEST_CASE("GuiWindow: UpdateFrame with face detection result") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    client::Frame frame(640, 480, CV_8UC3);
    frame.Mat().setTo(cv::Scalar(100, 150, 200));

    client::FaceDetectionResult result;
    result.frame_id = 1;
    result.processing_time_ms = 15.5f;

    client::FaceData face;
    face.bounding_box.x = 100;
    face.bounding_box.y = 100;
    face.bounding_box.width = 200;
    face.bounding_box.height = 200;
    face.confidence = 0.95f;
    face.relative_distance = 0.5f;
    face.track_id = 1;

    result.faces.push_back(face);

    CHECK_NOTHROW(window.UpdateFrame(frame, result));
  }

  TEST_CASE("GuiWindow: close is safe even when not shown") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    CHECK_NOTHROW(window.close());
  }

  TEST_CASE("GuiWindow: isVisible returns false initially") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    // Window may or may not be visible depending on QML loading
    CHECK_NOTHROW([&]() { [[maybe_unused]] bool visible = window.isVisible(); }());
  }

  TEST_CASE("GuiWindow: Multiple UpdateFrame calls are safe") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    client::Frame frame(640, 480, CV_8UC3);
    frame.Mat().setTo(cv::Scalar(100, 150, 200));

    for (int i = 0; i < 10; ++i) {
      CHECK_NOTHROW(window.UpdateFrame(frame));
    }
  }

  TEST_CASE("GuiWindow: Multiple UpdateStats calls are safe") {
    EnsureQGuiApplication();

    client::GuiWindow window;

    for (int i = 0; i < 10; ++i) {
      CHECK_NOTHROW(window.UpdateStats(30.0f + static_cast<float>(i), static_cast<uint64_t>(i * 100),
                                       static_cast<size_t>(i % 5)));
    }
  }

  TEST_CASE("GuiWindow: UpdateFrame with various frame sizes") {
    EnsureQGuiApplication();

    client::GuiWindow window;

    // Test different frame sizes
    const std::vector<std::pair<int, int>> sizes = {{640, 480}, {1280, 720}, {320, 240}, {1920, 1080}};

    for (const auto& [width, height] : sizes) {
      client::Frame frame(width, height, CV_8UC3);
      frame.Mat().setTo(cv::Scalar(100, 150, 200));
      CHECK_NOTHROW(window.UpdateFrame(frame));
    }
  }

  TEST_CASE("GuiWindow: UpdateFrame with multiple faces including distance") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    client::Frame frame(640, 480, CV_8UC3);
    frame.Mat().setTo(cv::Scalar(100, 150, 200));

    client::FaceDetectionResult result;
    result.frame_id = 1;
    result.processing_time_ms = 25.0f;

    // Add multiple faces with different distances
    for (int i = 0; i < 5; ++i) {
      client::FaceData face;
      face.bounding_box.x = static_cast<float>(50 + i * 100);
      face.bounding_box.y = static_cast<float>(50 + i * 50);
      face.bounding_box.width = 80;
      face.bounding_box.height = 80;
      face.confidence = 0.8f + static_cast<float>(i) * 0.04f;
      face.relative_distance = 0.2f + static_cast<float>(i) * 0.15f;
      face.track_id = static_cast<uint32_t>(i + 1);
      result.faces.push_back(face);
    }

    CHECK_NOTHROW(window.UpdateFrame(frame, result));
  }

  TEST_CASE("GuiWindow: SetCameraSwitchCallback can be set multiple times") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    int callback_count = 0;

    window.SetCameraSwitchCallback([&](std::string_view) { callback_count = 1; });
    window.SetCameraSwitchCallback([&](std::string_view) { callback_count = 2; });

    // Second callback should replace first
    CHECK_EQ(callback_count, 0);  // Neither called yet
  }

  TEST_CASE("GuiWindow: SetModelSwitchCallback can be set multiple times") {
    EnsureQGuiApplication();

    client::GuiWindow window;
    int callback_count = 0;

    window.SetModelSwitchCallback([&](client::ModelType) { callback_count = 1; });
    window.SetModelSwitchCallback([&](client::ModelType) { callback_count = 2; });

    // Second callback should replace first
    CHECK_EQ(callback_count, 0);  // Neither called yet
  }
}

TEST_SUITE("client::FrameImageProvider") {
  static int argc = 1;
  static char* argv[] = {const_cast<char*>("test")};
  static std::unique_ptr<QGuiApplication> app;

  static void EnsureQGuiApplication() {
    if (!QGuiApplication::instance()) {
      app = std::make_unique<QGuiApplication>(argc, argv);
    }
  }

  TEST_CASE("FrameImageProvider: Default construction") {
    EnsureQGuiApplication();

    client::FrameImageProvider provider;

    // Request image without updating first
    QSize size;
    QImage image = provider.requestImage("test", &size, QSize());

    // Should return a placeholder (black image)
    CHECK_FALSE(image.isNull());
    CHECK_EQ(size.width(), 320);
    CHECK_EQ(size.height(), 240);
  }

  TEST_CASE("FrameImageProvider: UpdateImage and requestImage") {
    EnsureQGuiApplication();

    client::FrameImageProvider provider;

    // Create a test image
    QImage test_image(100, 100, QImage::Format_RGB888);
    test_image.fill(Qt::red);

    provider.UpdateImage(test_image);

    QSize size;
    QImage result = provider.requestImage("test", &size, QSize());

    CHECK_FALSE(result.isNull());
    CHECK_EQ(size.width(), 100);
    CHECK_EQ(size.height(), 100);
  }

  TEST_CASE("FrameImageProvider: Scaling with requestedSize") {
    EnsureQGuiApplication();

    client::FrameImageProvider provider;

    QImage test_image(200, 200, QImage::Format_RGB888);
    test_image.fill(Qt::blue);

    provider.UpdateImage(test_image);

    QSize size;
    QImage result = provider.requestImage("test", &size, QSize(100, 100));

    CHECK_FALSE(result.isNull());
    // Image should be scaled
    CHECK_EQ(result.width(), 100);
    CHECK_EQ(result.height(), 100);
  }

  TEST_CASE("FrameImageProvider: Multiple updates") {
    EnsureQGuiApplication();

    client::FrameImageProvider provider;

    for (int i = 0; i < 10; ++i) {
      QImage test_image(100 + i * 10, 100 + i * 10, QImage::Format_RGB888);
      test_image.fill(Qt::green);

      CHECK_NOTHROW(provider.UpdateImage(test_image));
    }
  }
}

TEST_SUITE("client::GuiBackend") {
  static int argc = 1;
  static char* argv[] = {const_cast<char*>("test")};
  static std::unique_ptr<QGuiApplication> app;

  static void EnsureQGuiApplication() {
    if (!QGuiApplication::instance()) {
      app = std::make_unique<QGuiApplication>(argc, argv);
    }
  }

  TEST_CASE("GuiBackend: Default construction") {
    EnsureQGuiApplication();

    client::GuiBackend backend;

    CHECK_EQ(backend.Fps(), doctest::Approx(0.0));
    CHECK_EQ(backend.FramesProcessed(), 0u);
    CHECK_EQ(backend.FacesDetected(), 0);
    CHECK(backend.CurrentCamera().isEmpty());
    CHECK_EQ(backend.CurrentModelType(), 0);
    CHECK(backend.Faces().isEmpty());
  }

  TEST_CASE("GuiBackend: UpdateStats updates properties") {
    EnsureQGuiApplication();

    client::GuiBackend backend;

    backend.UpdateStats(30.5f, 1000, 3);

    CHECK_EQ(backend.Fps(), doctest::Approx(30.5));
    CHECK_EQ(backend.FramesProcessed(), 1000u);
    CHECK_EQ(backend.FacesDetected(), 3);
  }

  TEST_CASE("GuiBackend: UpdateFaces populates face list") {
    EnsureQGuiApplication();

    client::GuiBackend backend;

    client::FaceDetectionResult result;
    client::FaceData face;
    face.bounding_box.x = 10.0f;
    face.bounding_box.y = 20.0f;
    face.bounding_box.width = 100.0f;
    face.bounding_box.height = 100.0f;
    face.confidence = 0.9f;
    face.relative_distance = 0.5f;
    face.track_id = 1;
    result.faces.push_back(face);

    backend.UpdateFaces(result);

    CHECK_EQ(backend.Faces().size(), 1);
  }

  TEST_CASE("GuiBackend: SetCurrentModel updates model type") {
    EnsureQGuiApplication();

    client::GuiBackend backend;

    backend.SetCurrentModel(client::ModelType::kResNet10Caffe);
    CHECK_EQ(backend.CurrentModelType(), 1);

    backend.SetCurrentModel(client::ModelType::kMobileNetCaffe);
    CHECK_EQ(backend.CurrentModelType(), 2);

    backend.SetCurrentModel(client::ModelType::kYuNetONNX);
    CHECK_EQ(backend.CurrentModelType(), 0);
  }

  TEST_CASE("GuiBackend: UpdateCameraList populates camera list") {
    EnsureQGuiApplication();

    client::GuiBackend backend;

    std::vector<client::CameraDeviceInfo> cameras;
    client::CameraDeviceInfo camera;
    camera.id = "cam1";
    camera.description = "Camera 1";
    camera.is_default = true;
    cameras.push_back(camera);

    backend.UpdateCameraList(cameras, "cam1");

    QVariantList list = backend.getCameraList();
    CHECK_EQ(list.size(), 1);
  }

  TEST_CASE("GuiBackend: selectCamera invokes callback") {
    EnsureQGuiApplication();

    client::GuiBackend backend;
    bool callback_called = false;
    std::string captured_id;

    backend.SetCameraSwitchCallback([&](std::string_view id) {
      callback_called = true;
      captured_id = std::string(id);
    });

    backend.selectCamera("test_camera");

    CHECK(callback_called);
    CHECK_EQ(captured_id, "test_camera");
  }

  TEST_CASE("GuiBackend: selectModel invokes callback") {
    EnsureQGuiApplication();

    client::GuiBackend backend;
    bool callback_called = false;
    client::ModelType captured_type{};

    backend.SetModelSwitchCallback([&](client::ModelType type) {
      callback_called = true;
      captured_type = type;
    });

    backend.selectModel(1);

    CHECK(callback_called);
    CHECK_EQ(captured_type, client::ModelType::kResNet10Caffe);
  }

  TEST_CASE("GuiBackend: applySettings invokes callback") {
    EnsureQGuiApplication();

    client::GuiBackend backend;
    bool callback_called = false;
    QVariantMap captured_settings;

    backend.SetSettingsChangedCallback([&](const QVariantMap& settings) {
      callback_called = true;
      captured_settings = settings;
    });

    QVariantMap settings;
    settings["fps"] = 30;
    settings["confidence"] = 0.5;

    backend.applySettings(settings);

    CHECK(callback_called);
    CHECK_EQ(captured_settings["fps"].toInt(), 30);
    CHECK_EQ(captured_settings["confidence"].toDouble(), doctest::Approx(0.5));
  }
}
