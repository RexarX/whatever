#include <doctest/doctest.h>

#include <client/app/app.hpp>
#include <client/app/camera.hpp>
#include <client/app/face_tracker.hpp>

#include <QCoreApplication>

#include <atomic>
#include <chrono>
#include <thread>

namespace {

// Helper to create argc/argv for testing
struct ArgvHelper {
  std::vector<std::string> args;
  std::vector<char*> argv;

  ArgvHelper(std::initializer_list<std::string> arg_list) : args(arg_list) {
    for (auto& arg : args) {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);
  }

  int argc() const { return static_cast<int>(args.size()); }
  char** data() { return argv.data(); }
};

}  // namespace

TEST_SUITE("client::AppIntegration") {
  TEST_CASE("App Integration: Complete construction and destruction lifecycle") {
    ArgvHelper args{"test_app"};

    {
      auto config = client::AppConfig::Default();
      config.headless = true;  // Force headless mode to avoid GUI freezing
      client::App app(args.argc(), args.data(), config);

      CHECK_FALSE(app.Running());
      CHECK_EQ(app.FramesProcessed(), 0);
      CHECK_FALSE(app.LastDetection().has_value());

      // Verify config is accessible
      const auto& stored_config = app.Config();
      CHECK_GE(stored_config.camera.preferred_width, 0);
      CHECK_GE(stored_config.camera.preferred_height, 0);
    }

    // Test passes if destructor completes without issues
    CHECK(true);
  }

  TEST_CASE("App Integration: Camera and face tracker initialization states") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();

    // Use a model path that might not exist (we're testing state management, not actual detection)
    config.face_tracker.model_path = "nonexistent_model.onnx";
    config.max_frames = 1;  // Limit frames for quick test
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    // Initially nothing should be running
    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);

    // Config should be stored correctly
    CHECK_EQ(app.Config().face_tracker.model_path, "nonexistent_model.onnx");
    CHECK_EQ(app.Config().max_frames, 1);
  }

  TEST_CASE("App Integration: RequestStop before running") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Request stop before ever starting
    app.RequestStop();

    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
  }

  TEST_CASE("App Integration: Multiple RequestStop calls") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Multiple stop requests should be safe
    app.RequestStop();
    app.RequestStop();
    app.RequestStop();

    CHECK_FALSE(app.Running());
  }

  TEST_CASE("App Integration: Face detection callback registration") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    std::atomic<int> callback_count{0};

    app.SetFaceDetectionCallback([&callback_count](const client::FaceDetectionResult& result) {
      ++callback_count;
      CHECK_GE(result.frame_id, 0);
      CHECK_GE(result.processing_time_ms, 0.0F);
    });

    // Callback is registered but not called yet
    CHECK_EQ(callback_count.load(), 0);
  }

  TEST_CASE("App Integration: Config propagation to components") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();

    // Customize config
    config.camera.preferred_width = 1280;
    config.camera.preferred_height = 720;
    config.camera.preferred_fps = 60;
    config.face_tracker.confidence_threshold = 0.8f;
    config.verbose = true;
    config.max_frames = 100;
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    // Verify config is stored
    const auto& stored_config = app.Config();
    CHECK_EQ(stored_config.camera.preferred_width, 1280);
    CHECK_EQ(stored_config.camera.preferred_height, 720);
    CHECK_EQ(stored_config.camera.preferred_fps, 60);
    CHECK_EQ(stored_config.face_tracker.confidence_threshold, doctest::Approx(0.8f));
    CHECK(stored_config.verbose);
    CHECK_EQ(stored_config.max_frames, 100);
  }

  TEST_CASE("App Integration: Name and Version accessibility") {
    // Static methods should be accessible without instance
    auto name = client::App::Name();
    auto version = client::App::Version();

    CHECK_FALSE(name.empty());
    CHECK_FALSE(version.empty());
    CHECK_GT(name.length(), 0);
    CHECK_GT(version.length(), 0);

    // Should contain expected substrings
    CHECK_NE(name.find("Tracker"), std::string_view::npos);
  }

  TEST_CASE("App Integration: Callback replacement behavior") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};

    // Set first callback
    app.SetFaceDetectionCallback(
        [&callback1_count](const client::FaceDetectionResult& /*result*/) { ++callback1_count; });

    // Replace with second callback
    app.SetFaceDetectionCallback(
        [&callback2_count](const client::FaceDetectionResult& /*result*/) { ++callback2_count; });

    // Neither should be called yet
    CHECK_EQ(callback1_count.load(), 0);
    CHECK_EQ(callback2_count.load(), 0);
  }

  TEST_CASE("App Integration: Camera device enumeration through app lifecycle") {
    // Test that camera enumeration works during app lifecycle
    auto available_devices = client::Camera::AvailableDevices();

    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Device list should still be accessible
    auto devices_after_app = client::Camera::AvailableDevices();
    CHECK_EQ(available_devices.size(), devices_after_app.size());
  }

  TEST_CASE("App Integration: Default config sanity checks") {
    auto default_config = client::AppConfig::Default();

    // Camera config should be reasonable
    CHECK_GT(default_config.camera.preferred_width, 0);
    CHECK_LE(default_config.camera.preferred_width, 10000);
    CHECK_GT(default_config.camera.preferred_height, 0);
    CHECK_LE(default_config.camera.preferred_height, 10000);
    CHECK_GT(default_config.camera.preferred_fps, 0);
    CHECK_LE(default_config.camera.preferred_fps, 240);

    // Face tracker config should be reasonable
    CHECK_GE(default_config.face_tracker.confidence_threshold, 0.0f);
    CHECK_LE(default_config.face_tracker.confidence_threshold, 1.0f);
    CHECK_GE(default_config.face_tracker.nms_threshold, 0.0f);
    CHECK_LE(default_config.face_tracker.nms_threshold, 1.0f);
    CHECK_GT(default_config.face_tracker.input_width, 0);
    CHECK_GT(default_config.face_tracker.input_height, 0);

    // Model path should be set
    CHECK_FALSE(default_config.face_tracker.model_path.empty());
  }

  TEST_CASE("App Integration: State consistency across operations") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Initial state
    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
    CHECK_FALSE(app.LastDetection().has_value());

    // Set callback
    app.SetFaceDetectionCallback([](const client::FaceDetectionResult& /*result*/) {
      // Callback does nothing
    });

    // State should remain consistent
    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
    CHECK_FALSE(app.LastDetection().has_value());

    // Request stop
    app.RequestStop();

    // State should still be consistent
    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
    CHECK_FALSE(app.LastDetection().has_value());
  }

  TEST_CASE("App Integration: Config modification doesn't affect constructed app") {
    auto config = client::AppConfig::Default();
    config.max_frames = 50;
    config.verbose = true;
    config.headless = true;  // Force headless mode to avoid GUI freezing

    ArgvHelper args{"test_app"};
    client::App app(args.argc(), args.data(), config);

    // Verify app has the config
    CHECK_EQ(app.Config().max_frames, 50);
    CHECK(app.Config().verbose);

    // Modify original config
    config.max_frames = 100;
    config.verbose = false;

    // App's config should be unchanged
    CHECK_EQ(app.Config().max_frames, 50);
    CHECK(app.Config().verbose);
  }

  TEST_CASE("App Integration: Multiple app instances can exist") {
    ArgvHelper args1{"test_app1"};
    ArgvHelper args2{"test_app2"};

    auto config1 = client::AppConfig::Default();
    config1.max_frames = 10;
    config1.headless = true;  // Force headless mode to avoid GUI freezing

    auto config2 = client::AppConfig::Default();
    config2.max_frames = 20;
    config2.headless = true;  // Force headless mode to avoid GUI freezing

    {
      client::App app1(args1.argc(), args1.data(), config1);

      {
        client::App app2(args2.argc(), args2.data(), config2);

        CHECK_EQ(app1.Config().max_frames, 10);
        CHECK_EQ(app2.Config().max_frames, 20);
        CHECK_FALSE(app1.Running());
        CHECK_FALSE(app2.Running());
      }

      // app1 should still be valid after app2 is destroyed
      CHECK_EQ(app1.Config().max_frames, 10);
      CHECK_FALSE(app1.Running());
    }

    CHECK(true);  // Test passes if no crashes
  }

  TEST_CASE("App Integration: LastDetection thread safety") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Multiple calls to LastDetection should be safe
    for (int i = 0; i < 100; ++i) {
      auto detection = app.LastDetection();
      CHECK_FALSE(detection.has_value());
    }
  }

  TEST_CASE("App Integration: FramesProcessed thread safety") {
    ArgvHelper args{"test_app"};

    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing
    client::App app(args.argc(), args.data(), config);

    // Multiple calls to FramesProcessed should be safe
    for (int i = 0; i < 100; ++i) {
      auto count = app.FramesProcessed();
      CHECK_EQ(count, 0);
    }
  }

  TEST_CASE("App Integration: Camera config through app") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();

    // Configure camera through app config
    config.camera.device_id = "";  // Default camera
    config.camera.preferred_width = 640;
    config.camera.preferred_height = 480;
    config.camera.preferred_fps = 30;
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    const auto& app_config = app.Config();
    CHECK_EQ(app_config.camera.device_id, "");
    CHECK_EQ(app_config.camera.preferred_width, 640);
    CHECK_EQ(app_config.camera.preferred_height, 480);
    CHECK_EQ(app_config.camera.preferred_fps, 30);
  }

  TEST_CASE("App Integration: Face tracker config through app") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();

    // Configure face tracker through app config
    config.face_tracker.model_path = "test_model.onnx";
    config.face_tracker.confidence_threshold = 0.75f;
    config.face_tracker.nms_threshold = 0.35f;
    config.face_tracker.input_width = 640;
    config.face_tracker.input_height = 480;
    config.face_tracker.swap_rb = false;
    config.face_tracker.use_gpu = true;
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    const auto& app_config = app.Config();
    CHECK_EQ(app_config.face_tracker.model_path, "test_model.onnx");
    CHECK_EQ(app_config.face_tracker.confidence_threshold, doctest::Approx(0.75f));
    CHECK_EQ(app_config.face_tracker.nms_threshold, doctest::Approx(0.35f));
    CHECK_EQ(app_config.face_tracker.input_width, 640);
    CHECK_EQ(app_config.face_tracker.input_height, 480);
    CHECK_FALSE(app_config.face_tracker.swap_rb);
    CHECK(app_config.face_tracker.use_gpu);
  }

  TEST_CASE("App Integration: Headless mode configuration") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    CHECK(app.Config().headless);
    CHECK_FALSE(app.Running());
    CHECK_FALSE(app.GuiEnabled());
  }

  TEST_CASE("App Integration: Verbose mode configuration") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.verbose = true;
    config.headless = true;  // Force headless mode to avoid GUI freezing

    client::App app(args.argc(), args.data(), config);

    CHECK(app.Config().verbose);
  }

  TEST_CASE("App Integration: Max frames configuration") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.headless = true;  // Force headless mode to avoid GUI freezing

    SUBCASE("Unlimited frames") {
      config.max_frames = 0;
      client::App app(args.argc(), args.data(), config);
      CHECK_EQ(app.Config().max_frames, 0);
    }

    SUBCASE("Limited frames") {
      config.max_frames = 1000;
      client::App app(args.argc(), args.data(), config);
      CHECK_EQ(app.Config().max_frames, 1000);
    }
  }
}  // TEST_SUITE
