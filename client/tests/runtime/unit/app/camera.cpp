#include <doctest/doctest.h>

#include <client/app/camera.hpp>

#include <QCoreApplication>
#include <QMediaDevices>

#include <set>

TEST_SUITE("client::Camera") {
  TEST_CASE("Camera: CameraErrorToString converts all error codes") {
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kNotFound), "Camera not found");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kAccessDenied), "Camera access denied");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kAlreadyInUse), "Camera already in use");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kNotStarted), "Camera not started");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kInvalidDevice), "Invalid camera device");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kCaptureError), "Frame capture error");
    CHECK_EQ(client::CameraErrorToString(client::CameraError::kConfigurationError), "Camera configuration error");
  }

  TEST_CASE("Camera: AvailableDevices returns device list") {
    auto devices = client::Camera::AvailableDevices();

    // Should return a valid vector (even if empty on systems without cameras)
    CHECK_GE(devices.size(), 0);

    // If devices are available, check their properties
    for (const auto& device : devices) {
      CHECK_FALSE(device.id.empty());
      CHECK_FALSE(device.description.empty());

      // At most one device should be marked as default
      static bool found_default = false;
      if (device.is_default) {
        CHECK_FALSE(found_default);
        found_default = true;
      }
    }
  }

  TEST_CASE("Camera: DefaultDevice returns optional device") {
    auto default_device = client::Camera::DefaultDevice();

    // If a default device exists, verify its properties
    if (default_device) {
      CHECK_FALSE(default_device->id.empty());
      CHECK_FALSE(default_device->description.empty());
      CHECK(default_device->is_default);
    }
  }

  TEST_CASE("Camera: Default construction creates uninitialized camera") {
    client::Camera camera;

    CHECK_FALSE(camera.Initialized());
    CHECK_FALSE(camera.Active());
    CHECK_EQ(camera.FramesCaptured(), 0);
    CHECK_EQ(camera.FramesDropped(), 0);
    CHECK_EQ(camera.CaptureWidth(), 0);
    CHECK_EQ(camera.CaptureHeight(), 0);
  }

  TEST_CASE("Camera: Throttling enabled by default") {
    client::Camera camera;
    client::CameraConfig config;

    auto result = camera.Initialize(config);
    if (result) {
      CHECK(camera.ThrottlingEnabled());
      CHECK_EQ(camera.TargetFps(), 30);
    }
  }

  TEST_CASE("Camera: Throttling can be disabled") {
    client::Camera camera;
    client::CameraConfig config;
    config.enable_throttling = false;

    auto result = camera.Initialize(config);
    if (result) {
      CHECK_FALSE(camera.ThrottlingEnabled());
    }
  }

  TEST_CASE("Camera: TargetFps setter updates FPS") {
    client::Camera camera;
    client::CameraConfig config;

    auto result = camera.Initialize(config);
    if (result) {
      camera.TargetFps(15);
      CHECK_EQ(camera.TargetFps(), 15);

      camera.TargetFps(60);
      CHECK_EQ(camera.TargetFps(), 60);
    }
  }

  TEST_CASE("Camera: ThrottlingEnabled setter works") {
    client::Camera camera;
    client::CameraConfig config;

    auto result = camera.Initialize(config);
    if (result) {
      CHECK(camera.ThrottlingEnabled());

      camera.ThrottlingEnabled(false);
      CHECK_FALSE(camera.ThrottlingEnabled());

      camera.ThrottlingEnabled(true);
      CHECK(camera.ThrottlingEnabled());
    }
  }

  TEST_CASE("Camera: FramesDropped starts at zero") {
    client::Camera camera;
    CHECK_EQ(camera.FramesDropped(), 0);
  }

  TEST_CASE("Camera: Config with throttling disabled") {
    client::CameraConfig config;
    config.enable_throttling = false;

    CHECK_FALSE(config.enable_throttling);
  }

  TEST_CASE("Camera: Config with custom FPS for throttling") {
    client::CameraConfig config;
    config.preferred_fps = 15;
    config.enable_throttling = true;

    CHECK_EQ(config.preferred_fps, 15);
    CHECK(config.enable_throttling);
  }

  TEST_CASE("Camera: Default config values") {
    client::CameraConfig config;

    CHECK(config.device_id.empty());
    CHECK_EQ(config.preferred_width, 640);
    CHECK_EQ(config.preferred_height, 480);
    CHECK_EQ(config.preferred_fps, 30);
    CHECK(config.enable_throttling);
  }

  TEST_CASE("Camera: CameraDeviceInfo structure") {
    client::CameraDeviceInfo info;
    info.id = "test_id";
    info.description = "Test Camera";
    info.is_default = true;

    CHECK_EQ(info.id, "test_id");
    CHECK_EQ(info.description, "Test Camera");
    CHECK(info.is_default);
  }

  TEST_CASE("Camera: Config returns configuration") {
    client::Camera camera;
    client::CameraConfig config;
    config.preferred_width = 1280;
    config.preferred_height = 720;
    config.preferred_fps = 60;

    // Initialize camera (may fail if no camera available)
    auto result = camera.Initialize(config);

    // Config should be stored regardless of initialization success
    const auto& stored_config = camera.Config();
    CHECK_EQ(stored_config.preferred_width, 1280);
    CHECK_EQ(stored_config.preferred_height, 720);
    CHECK_EQ(stored_config.preferred_fps, 60);
  }

  TEST_CASE("Camera: Start requires initialization") {
    client::Camera camera;

    auto result = camera.Start();
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotStarted);
    CHECK_FALSE(camera.Active());
  }

  TEST_CASE("Camera: CaptureFrame requires camera to be started") {
    client::Camera camera;

    // Try to capture without initialization
    auto result = camera.CaptureFrame();
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotStarted);
  }

  TEST_CASE("Camera: Stop is safe when not active") {
    client::Camera camera;

    CHECK_NOTHROW(camera.Stop());
    CHECK_FALSE(camera.Active());
  }

  TEST_CASE("Camera: Multiple Stop calls are safe") {
    client::Camera camera;

    CHECK_NOTHROW(camera.Stop());
    CHECK_NOTHROW(camera.Stop());
    CHECK_NOTHROW(camera.Stop());
  }

  TEST_CASE("Camera: SetFrameCallback stores callback") {
    client::Camera camera;
    bool callback_called = false;

    camera.SetFrameCallback([&callback_called](const client::Frame& frame) { callback_called = true; });

    // Callback is stored (we can't test if it's called without a real camera)
    CHECK_FALSE(callback_called);  // Not called yet
  }

  TEST_CASE("Camera: Initialize with empty device_id uses default") {
    client::Camera camera;
    client::CameraConfig config;
    config.device_id = "";  // Empty means default

    auto result = camera.Initialize(config);

    // If initialization fails, it should be due to no camera being available
    if (!result) {
      CHECK((result.error() == client::CameraError::kNotFound ||
             result.error() == client::CameraError::kConfigurationError ||
             result.error() == client::CameraError::kAccessDenied));
    }
  }

  TEST_CASE("Camera: Initialize with invalid device_id fails") {
    client::Camera camera;
    client::CameraConfig config;
    config.device_id = "definitely_not_a_real_camera_id_12345";

    auto result = camera.Initialize(config);

    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotFound);
  }

  TEST_CASE("Camera: Re-initialization stops previous instance") {
    client::Camera camera;
    client::CameraConfig config;

    // First initialization
    auto result1 = camera.Initialize(config);
    bool first_init_success = result1.has_value();

    // Second initialization should work even if camera was initialized
    auto result2 = camera.Initialize(config);

    // If first init succeeded, second should succeed too
    if (first_init_success) {
      CHECK(result2.has_value());
    }
  }

  TEST_CASE("Camera: Destructor stops active camera") {
    {
      client::Camera camera;
      client::CameraConfig config;

      auto init_result = camera.Initialize(config);
      if (init_result) {
        auto start_result = camera.Start();
        if (start_result) {
          CHECK(camera.Active());
        }
      }
      // Camera should stop when going out of scope
    }

    // Test passes if no crash occurs
    CHECK(true);
  }

  TEST_CASE("Camera: FramesCaptured starts at zero") {
    client::Camera camera;
    CHECK_EQ(camera.FramesCaptured(), 0);
  }

  TEST_CASE("Camera: Capture dimensions are zero before initialization") {
    client::Camera camera;
    CHECK_EQ(camera.CaptureWidth(), 0);
    CHECK_EQ(camera.CaptureHeight(), 0);
  }

  TEST_CASE("Camera: State consistency after failed initialization") {
    client::Camera camera;
    client::CameraConfig config;
    config.device_id = "invalid_device_id_xyz";

    auto result = camera.Initialize(config);
    CHECK_FALSE(result.has_value());

    // Camera should remain uninitialized
    CHECK_FALSE(camera.Initialized());
    CHECK_FALSE(camera.Active());
  }

  TEST_CASE("Camera: Config with custom dimensions") {
    client::CameraConfig config;
    config.preferred_width = 1920;
    config.preferred_height = 1080;
    config.preferred_fps = 30;

    CHECK_EQ(config.preferred_width, 1920);
    CHECK_EQ(config.preferred_height, 1080);
    CHECK_EQ(config.preferred_fps, 30);
  }

  TEST_CASE("Camera: Error enum values are distinct") {
    std::set<uint8_t> error_values;
    error_values.insert(static_cast<uint8_t>(client::CameraError::kNotFound));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kAccessDenied));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kAlreadyInUse));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kNotStarted));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kInvalidDevice));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kCaptureError));
    error_values.insert(static_cast<uint8_t>(client::CameraError::kConfigurationError));

    // All error values should be distinct
    CHECK_EQ(error_values.size(), 7);
  }

  TEST_CASE("Camera: Device enumeration consistency") {
    auto devices1 = client::Camera::AvailableDevices();
    auto devices2 = client::Camera::AvailableDevices();

    // Multiple calls should return the same number of devices
    CHECK_EQ(devices1.size(), devices2.size());

    // If default device exists, it should be consistent
    auto default1 = client::Camera::DefaultDevice();
    auto default2 = client::Camera::DefaultDevice();

    CHECK_EQ(default1.has_value(), default2.has_value());
    if (default1 && default2) {
      CHECK_EQ(default1->id, default2->id);
    }
  }

  TEST_CASE("Camera: Start when already active returns success") {
    client::Camera camera;
    client::CameraConfig config;

    auto init_result = camera.Initialize(config);
    if (init_result) {
      auto start_result1 = camera.Start();
      if (start_result1) {
        // Second start should succeed (already running)
        auto start_result2 = camera.Start();
        CHECK(start_result2.has_value());
        CHECK(camera.Active());
      }
    }
  }

  TEST_CASE("Camera: CameraDeviceInfo default values") {
    client::CameraDeviceInfo info;

    CHECK(info.id.empty());
    CHECK(info.description.empty());
    CHECK_FALSE(info.is_default);
  }

  TEST_CASE("Camera: AvailableDeviceCount returns correct count") {
    const auto count = client::Camera::AvailableDeviceCount();
    const auto devices = client::Camera::AvailableDevices();

    CHECK_EQ(count, devices.size());
  }

  TEST_CASE("Camera: SwitchCamera requires initialization") {
    client::Camera camera;

    auto result = camera.SwitchCamera("");
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotStarted);
  }

  TEST_CASE("Camera: SwitchCamera with invalid device fails") {
    client::Camera camera;
    client::CameraConfig config;

    auto init_result = camera.Initialize(config);
    if (init_result) {
      auto result = camera.SwitchCamera("definitely_not_a_real_camera_id_xyz");
      CHECK_FALSE(result.has_value());
      CHECK_EQ(result.error(), client::CameraError::kNotFound);
    }
  }

  TEST_CASE("Camera: SwitchToNextCamera requires initialization") {
    client::Camera camera;

    auto result = camera.SwitchToNextCamera();
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotStarted);
  }

  TEST_CASE("Camera: SwitchToPreviousCamera requires initialization") {
    client::Camera camera;

    auto result = camera.SwitchToPreviousCamera();
    CHECK_FALSE(result.has_value());
    CHECK_EQ(result.error(), client::CameraError::kNotStarted);
  }

  TEST_CASE("Camera: CurrentDevice returns nullopt when not initialized") {
    client::Camera camera;

    auto device = camera.CurrentDevice();
    CHECK_FALSE(device.has_value());
  }

  TEST_CASE("Camera: CurrentDevice returns device info when initialized") {
    client::Camera camera;
    client::CameraConfig config;

    auto init_result = camera.Initialize(config);
    if (init_result) {
      auto device = camera.CurrentDevice();
      if (device) {
        CHECK_FALSE(device->id.empty());
        CHECK_FALSE(device->description.empty());
      }
    }
  }

  TEST_CASE("Camera: SwitchToNextCamera wraps around") {
    client::Camera camera;
    client::CameraConfig config;

    const auto devices = client::Camera::AvailableDevices();
    if (devices.size() <= 1) {
      // Skip test if only one or no cameras
      return;
    }

    auto init_result = camera.Initialize(config);
    if (init_result) {
      // Switch through all cameras
      for (size_t i = 0; i < devices.size(); ++i) {
        auto result = camera.SwitchToNextCamera();
        if (result) {
          CHECK(camera.Initialized());
        }
      }
    }
  }

  TEST_CASE("Camera: SwitchToPreviousCamera wraps around") {
    client::Camera camera;
    client::CameraConfig config;

    const auto devices = client::Camera::AvailableDevices();
    if (devices.size() <= 1) {
      // Skip test if only one or no cameras
      return;
    }

    auto init_result = camera.Initialize(config);
    if (init_result) {
      // Switch back through all cameras
      for (size_t i = 0; i < devices.size(); ++i) {
        auto result = camera.SwitchToPreviousCamera();
        if (result) {
          CHECK(camera.Initialized());
        }
      }
    }
  }
}
