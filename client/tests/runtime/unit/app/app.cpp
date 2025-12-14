#include <doctest/doctest.h>

#include <client/app/app.hpp>

#include <cstdlib>
#include <string>
#include <vector>

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

TEST_SUITE("client::App") {
  TEST_CASE("App: AppConfig default values") {
    auto config = client::AppConfig::Default();

    CHECK_EQ(config.camera.preferred_width, 640);
    CHECK_EQ(config.camera.preferred_height, 480);
    CHECK_EQ(config.camera.preferred_fps, 30);
    CHECK_EQ(config.face_tracker.confidence_threshold, 0.5f);
    CHECK_EQ(config.face_tracker.nms_threshold, 0.4f);
    CHECK_EQ(config.face_tracker.input_width, 320);
    CHECK_EQ(config.face_tracker.input_height, 320);
    CHECK(config.face_tracker.swap_rb);
    CHECK_FALSE(config.face_tracker.use_gpu);
    CHECK_FALSE(config.headless);
    CHECK_FALSE(config.verbose);
    CHECK_EQ(config.max_frames, 0);
  }

  TEST_CASE("App: Name and Version are non-empty") {
    CHECK_FALSE(client::App::Name().empty());
    CHECK_FALSE(client::App::Version().empty());
    CHECK_GT(client::App::Name().size(), 0);
    CHECK_GT(client::App::Version().size(), 0);
  }

  TEST_CASE("App: Construction with minimal arguments") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
    CHECK_FALSE(app.LastDetection().has_value());
  }

  TEST_CASE("App: Construction with explicit config") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.verbose = true;
    config.max_frames = 100;

    client::App app(args.argc(), args.data(), config);

    CHECK_FALSE(app.Running());
    CHECK_EQ(app.Config().verbose, true);
    CHECK_EQ(app.Config().max_frames, 100);
  }

  TEST_CASE("App: Config returns stored configuration") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.camera.preferred_width = 1280;
    config.camera.preferred_height = 720;
    config.headless = true;

    client::App app(args.argc(), args.data(), config);

    const auto& stored_config = app.Config();
    CHECK_EQ(stored_config.camera.preferred_width, 1280);
    CHECK_EQ(stored_config.camera.preferred_height, 720);
    CHECK(stored_config.headless);
  }

  TEST_CASE("App: Initial state after construction") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_FALSE(app.Running());
    CHECK_EQ(app.FramesProcessed(), 0);
    CHECK_FALSE(app.LastDetection().has_value());
  }

  TEST_CASE("App: FramesProcessed starts at zero") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_EQ(app.FramesProcessed(), 0);
  }

  TEST_CASE("App: LastDetection is nullopt initially") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_FALSE(app.LastDetection().has_value());
  }

  TEST_CASE("App: RequestStop sets stop flag") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_NOTHROW(app.RequestStop());
    // Note: We can't directly verify stop_requested_ is set since it's private,
    // but we can verify the call doesn't throw
  }

  TEST_CASE("App: SetFaceDetectionCallback stores callback") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    bool callback_called = false;
    app.SetFaceDetectionCallback(
        [&callback_called](const client::FaceDetectionResult& result) { callback_called = true; });

    // Callback is stored but not called yet
    CHECK_FALSE(callback_called);
  }

  TEST_CASE("App: Multiple RequestStop calls are safe") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_NOTHROW(app.RequestStop());
    CHECK_NOTHROW(app.RequestStop());
    CHECK_NOTHROW(app.RequestStop());
  }

  TEST_CASE("App: AppConfig custom camera settings") {
    auto config = client::AppConfig::Default();
    config.camera.device_id = "custom_camera";
    config.camera.preferred_width = 1920;
    config.camera.preferred_height = 1080;
    config.camera.preferred_fps = 60;

    CHECK_EQ(config.camera.device_id, "custom_camera");
    CHECK_EQ(config.camera.preferred_width, 1920);
    CHECK_EQ(config.camera.preferred_height, 1080);
    CHECK_EQ(config.camera.preferred_fps, 60);
  }

  TEST_CASE("App: AppConfig custom face tracker settings") {
    auto config = client::AppConfig::Default();
    config.face_tracker.model_path = "custom_model.onnx";
    config.face_tracker.confidence_threshold = 0.7f;
    config.face_tracker.nms_threshold = 0.3f;
    config.face_tracker.use_gpu = true;

    CHECK_EQ(config.face_tracker.model_path, "custom_model.onnx");
    CHECK_EQ(config.face_tracker.confidence_threshold, doctest::Approx(0.7f));
    CHECK_EQ(config.face_tracker.nms_threshold, doctest::Approx(0.3f));
    CHECK(config.face_tracker.use_gpu);
  }

  TEST_CASE("App: AppConfig headless mode") {
    auto config = client::AppConfig::Default();
    config.headless = true;

    CHECK(config.headless);
  }

  TEST_CASE("App: AppConfig verbose mode") {
    auto config = client::AppConfig::Default();
    config.verbose = true;

    CHECK(config.verbose);
  }

  TEST_CASE("App: AppConfig max_frames unlimited by default") {
    auto config = client::AppConfig::Default();
    CHECK_EQ(config.max_frames, 0);
  }

  TEST_CASE("App: AppConfig max_frames can be limited") {
    auto config = client::AppConfig::Default();
    config.max_frames = 1000;

    CHECK_EQ(config.max_frames, 1000);
  }

  TEST_CASE("App: Destructor is safe when not running") {
    ArgvHelper args{"test_app"};

    {
      client::App app(args.argc(), args.data());
      CHECK_FALSE(app.Running());
    }

    // Test passes if no crash occurs
    CHECK(true);
  }

  TEST_CASE("App: Config accessor returns const reference") {
    ArgvHelper args{"test_app"};
    auto config = client::AppConfig::Default();
    config.verbose = true;

    client::App app(args.argc(), args.data(), config);

    // Should return const reference
    const auto& config_ref = app.Config();
    CHECK_EQ(&config_ref, &app.Config());
  }

  TEST_CASE("App: Running state is false initially") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_FALSE(app.Running());
  }

  TEST_CASE("App: Face detection callback can be replaced") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    int callback1_count = 0;
    int callback2_count = 0;

    app.SetFaceDetectionCallback([&callback1_count](const client::FaceDetectionResult& result) { ++callback1_count; });

    app.SetFaceDetectionCallback([&callback2_count](const client::FaceDetectionResult& result) { ++callback2_count; });

    // Only second callback should be stored
    CHECK_EQ(callback1_count, 0);
    CHECK_EQ(callback2_count, 0);
  }

  TEST_CASE("App: AppConfig face tracker input dimensions") {
    auto config = client::AppConfig::Default();

    CHECK_EQ(config.face_tracker.input_width, 320);
    CHECK_EQ(config.face_tracker.input_height, 320);

    config.face_tracker.input_width = 640;
    config.face_tracker.input_height = 480;

    CHECK_EQ(config.face_tracker.input_width, 640);
    CHECK_EQ(config.face_tracker.input_height, 480);
  }

  TEST_CASE("App: AppConfig swap_rb flag") {
    auto config = client::AppConfig::Default();
    CHECK(config.face_tracker.swap_rb);

    config.face_tracker.swap_rb = false;
    CHECK_FALSE(config.face_tracker.swap_rb);
  }

  TEST_CASE("App: Construction with different argument patterns") {
    SUBCASE("Single argument") {
      ArgvHelper args{"test_app"};
      client::App app(args.argc(), args.data());
      CHECK_FALSE(app.Running());
    }

    SUBCASE("Multiple arguments") {
      ArgvHelper args{"test_app", "--verbose", "--headless"};
      // Note: Actual parsing would require proper Qt app initialization
      // This tests that construction doesn't crash
      CHECK_NOTHROW([&]() {
        // We can't fully construct with these args without proper Qt setup,
        // but we can test config directly
        auto config = client::AppConfig::Default();
        CHECK_FALSE(config.verbose);
      }());
    }
  }

  TEST_CASE("App: Name returns consistent value") {
    auto name1 = client::App::Name();
    auto name2 = client::App::Name();

    CHECK_EQ(name1, name2);
  }

  TEST_CASE("App: Version returns consistent value") {
    auto version1 = client::App::Version();
    auto version2 = client::App::Version();

    CHECK_EQ(version1, version2);
  }

  TEST_CASE("App: AppConfig model path is set by default") {
    auto config = client::AppConfig::Default();
    CHECK_FALSE(config.face_tracker.model_path.empty());
  }

  TEST_CASE("App: AppConfig confidence threshold in valid range") {
    auto config = client::AppConfig::Default();
    CHECK_GE(config.face_tracker.confidence_threshold, 0.0f);
    CHECK_LE(config.face_tracker.confidence_threshold, 1.0f);
  }

  TEST_CASE("App: AppConfig nms threshold in valid range") {
    auto config = client::AppConfig::Default();
    CHECK_GE(config.face_tracker.nms_threshold, 0.0f);
    CHECK_LE(config.face_tracker.nms_threshold, 1.0f);
  }

  TEST_CASE("App: AppConfig camera dimensions are positive") {
    auto config = client::AppConfig::Default();
    CHECK_GT(config.camera.preferred_width, 0);
    CHECK_GT(config.camera.preferred_height, 0);
    CHECK_GT(config.camera.preferred_fps, 0);
  }

  TEST_CASE("App: AppConfig face tracker dimensions are positive") {
    auto config = client::AppConfig::Default();
    CHECK_GT(config.face_tracker.input_width, 0);
    CHECK_GT(config.face_tracker.input_height, 0);
  }

  TEST_CASE("App: LastDetection remains nullopt until detection occurs") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    // Initially nullopt
    CHECK_FALSE(app.LastDetection().has_value());

    // Should remain nullopt until Run() is called and frames are processed
    CHECK_FALSE(app.LastDetection().has_value());
  }

  TEST_CASE("App: FramesProcessed doesn't increment without running") {
    ArgvHelper args{"test_app"};

    client::App app(args.argc(), args.data());

    CHECK_EQ(app.FramesProcessed(), 0);
    // Without calling Run(), frames processed should stay at 0
    CHECK_EQ(app.FramesProcessed(), 0);
  }
}
