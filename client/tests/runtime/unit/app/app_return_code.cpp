#include <doctest/doctest.h>

#include <client/app/app_return_code.hpp>

TEST_SUITE("client::AppReturnCode") {
  TEST_CASE("AppReturnCodeToString: Returns correct strings") {
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kSuccess), "Success");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kCameraInitFailed), "Camera initialization failed");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kFaceTrackerInitFailed),
             "Face tracker initialization failed");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kModelLoadFailed), "Model load failed");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kFrameCaptureError), "Frame capture error");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kInvalidConfiguration), "Invalid configuration");
    CHECK_EQ(client::AppReturnCodeToString(client::AppReturnCode::kUnknownError), "Unknown error");
  }

  TEST_CASE("IsSuccess: Returns true only for kSuccess") {
    CHECK(client::IsSuccess(client::AppReturnCode::kSuccess));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kCameraInitFailed));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kFaceTrackerInitFailed));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kModelLoadFailed));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kFrameCaptureError));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kInvalidConfiguration));
    CHECK_FALSE(client::IsSuccess(client::AppReturnCode::kUnknownError));
  }

  TEST_CASE("ToExitCode: Returns correct integer values") {
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kSuccess), 0);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kCameraInitFailed), 1);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kFaceTrackerInitFailed), 2);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kModelLoadFailed), 3);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kFrameCaptureError), 4);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kInvalidConfiguration), 5);
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kUnknownError), 255);
  }

  TEST_CASE("AppReturnCode: Enum values are distinct") {
    CHECK_NE(client::AppReturnCode::kSuccess, client::AppReturnCode::kCameraInitFailed);
    CHECK_NE(client::AppReturnCode::kCameraInitFailed, client::AppReturnCode::kFaceTrackerInitFailed);
    CHECK_NE(client::AppReturnCode::kFaceTrackerInitFailed, client::AppReturnCode::kModelLoadFailed);
    CHECK_NE(client::AppReturnCode::kModelLoadFailed, client::AppReturnCode::kFrameCaptureError);
    CHECK_NE(client::AppReturnCode::kFrameCaptureError, client::AppReturnCode::kInvalidConfiguration);
    CHECK_NE(client::AppReturnCode::kInvalidConfiguration, client::AppReturnCode::kUnknownError);
  }

  TEST_CASE("AppReturnCode: Constexpr functions") {
    // Verify functions are constexpr by using them in constexpr contexts
    constexpr auto success_str = client::AppReturnCodeToString(client::AppReturnCode::kSuccess);
    constexpr bool is_success = client::IsSuccess(client::AppReturnCode::kSuccess);
    constexpr int exit_code = client::ToExitCode(client::AppReturnCode::kSuccess);

    CHECK_EQ(success_str, "Success");
    CHECK(is_success);
    CHECK_EQ(exit_code, 0);
  }

  TEST_CASE("AppReturnCode: kSuccess has exit code 0 for shell compatibility") {
    // This is important for proper shell exit code handling
    CHECK_EQ(client::ToExitCode(client::AppReturnCode::kSuccess), 0);
  }

  TEST_CASE("AppReturnCode: Error codes are non-zero") {
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kCameraInitFailed), 0);
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kFaceTrackerInitFailed), 0);
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kModelLoadFailed), 0);
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kFrameCaptureError), 0);
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kInvalidConfiguration), 0);
    CHECK_NE(client::ToExitCode(client::AppReturnCode::kUnknownError), 0);
  }
}
