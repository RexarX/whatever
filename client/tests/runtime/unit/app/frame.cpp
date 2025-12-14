#include <doctest/doctest.h>

#include <client/app/frame.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

TEST_SUITE("client::Frame") {
  TEST_CASE("Frame: Default construction creates empty frame") {
    client::Frame frame;

    CHECK(frame.Empty());
    CHECK_FALSE(frame);
    CHECK_EQ(frame.Width(), 0);
    CHECK_EQ(frame.Height(), 0);
    CHECK_EQ(frame.TotalPixels(), 0);
  }

  TEST_CASE("Frame: Construction from cv::Mat") {
    cv::Mat mat(480, 640, CV_8UC3, cv::Scalar(255, 0, 0));
    client::Frame frame(mat);

    CHECK_FALSE(frame.Empty());
    CHECK(frame);
    CHECK_EQ(frame.Width(), 640);
    CHECK_EQ(frame.Height(), 480);
    CHECK_EQ(frame.Channels(), 3);
    CHECK_EQ(frame.TotalPixels(), 640 * 480);
    CHECK_EQ(frame.Type(), CV_8UC3);
  }

  TEST_CASE("Frame: Construction with dimensions") {
    client::Frame frame(320, 240, CV_8UC1);

    CHECK_FALSE(frame.Empty());
    CHECK_EQ(frame.Width(), 320);
    CHECK_EQ(frame.Height(), 240);
    CHECK_EQ(frame.Channels(), 1);
    CHECK_EQ(frame.Type(), CV_8UC1);
  }

  TEST_CASE("Frame: Copy construction creates deep copy") {
    cv::Mat mat(100, 100, CV_8UC3, cv::Scalar(128, 128, 128));
    client::Frame original(mat);
    client::Frame copy(original);

    CHECK_FALSE(copy.Empty());
    CHECK_EQ(copy.Width(), original.Width());
    CHECK_EQ(copy.Height(), original.Height());

    // Modify original and verify copy is unaffected
    original.Mat().setTo(cv::Scalar(0, 0, 0));
    CHECK_NE(copy.Mat().at<cv::Vec3b>(0, 0)[0], 0);
  }

  TEST_CASE("Frame: Move construction") {
    cv::Mat mat(100, 100, CV_8UC3, cv::Scalar(64, 64, 64));
    client::Frame original(mat);
    const int original_width = original.Width();

    client::Frame moved(std::move(original));

    CHECK_FALSE(moved.Empty());
    CHECK_EQ(moved.Width(), original_width);
    CHECK(original.Empty());
  }

  TEST_CASE("Frame: Copy assignment creates deep copy") {
    cv::Mat mat1(50, 50, CV_8UC3, cv::Scalar(100, 100, 100));
    cv::Mat mat2(75, 75, CV_8UC3, cv::Scalar(200, 200, 200));

    client::Frame frame1(mat1);
    client::Frame frame2(mat2);

    frame2 = frame1;

    CHECK_EQ(frame2.Width(), 50);
    CHECK_EQ(frame2.Height(), 50);

    // Modify frame1 and verify frame2 is unaffected
    frame1.Mat().setTo(cv::Scalar(0, 0, 0));
    CHECK_NE(frame2.Mat().at<cv::Vec3b>(0, 0)[0], 0);
  }

  TEST_CASE("Frame: Move assignment") {
    cv::Mat mat1(50, 50, CV_8UC3);
    cv::Mat mat2(75, 75, CV_8UC3);

    client::Frame frame1(mat1);
    client::Frame frame2(mat2);

    frame2 = std::move(frame1);

    CHECK_EQ(frame2.Width(), 50);
    CHECK_EQ(frame2.Height(), 50);
    CHECK(frame1.Empty());
  }

  TEST_CASE("Frame: Clone creates independent copy") {
    cv::Mat mat(100, 100, CV_8UC3, cv::Scalar(50, 100, 150));
    client::Frame original(mat);

    client::Frame cloned = original.Clone();

    CHECK_FALSE(cloned.Empty());
    CHECK_EQ(cloned.Width(), original.Width());
    CHECK_EQ(cloned.Height(), original.Height());

    // Verify data is independent
    original.Mat().setTo(cv::Scalar(0, 0, 0));
    CHECK_EQ(cloned.Mat().at<cv::Vec3b>(0, 0)[0], 50);
  }

  TEST_CASE("Frame: ConvertColor changes color space") {
    cv::Mat bgr(100, 100, CV_8UC3, cv::Scalar(255, 0, 0));  // Blue in BGR
    client::Frame frame(bgr);

    client::Frame rgb_frame = frame.ConvertColor(cv::COLOR_BGR2RGB);

    CHECK_FALSE(rgb_frame.Empty());
    CHECK_EQ(rgb_frame.Channels(), 3);

    // Blue in BGR should be Red in RGB
    cv::Vec3b rgb_pixel = rgb_frame.Mat().at<cv::Vec3b>(0, 0);
    CHECK_EQ(rgb_pixel[0], 0);    // R
    CHECK_EQ(rgb_pixel[1], 0);    // G
    CHECK_EQ(rgb_pixel[2], 255);  // B
  }

  TEST_CASE("Frame: Resize changes dimensions") {
    cv::Mat mat(480, 640, CV_8UC3);
    client::Frame frame(mat);

    client::Frame resized = frame.Resize(320, 240);

    CHECK_FALSE(resized.Empty());
    CHECK_EQ(resized.Width(), 320);
    CHECK_EQ(resized.Height(), 240);
    CHECK_EQ(resized.Channels(), 3);
  }

  TEST_CASE("Frame: Data access") {
    cv::Mat mat(100, 100, CV_8UC3, cv::Scalar(1, 2, 3));
    client::Frame frame(mat);

    auto data = frame.Data();
    CHECK_FALSE(data.empty());
    CHECK_EQ(data.size(), 100 * 100 * 3);
    CHECK_EQ(data[0], 1);
    CHECK_EQ(data[1], 2);
    CHECK_EQ(data[2], 3);

    // Const version
    const client::Frame& const_frame = frame;
    auto const_data = const_frame.Data();
    CHECK_FALSE(const_data.empty());
    CHECK_EQ(const_data.size(), 100 * 100 * 3);
  }

  TEST_CASE("Frame: Step returns row stride") {
    cv::Mat mat(100, 100, CV_8UC3);
    client::Frame frame(mat);

    // Step should be at least width * channels
    CHECK_GE(frame.Step(), static_cast<size_t>(100 * 3));
  }

  TEST_CASE("Frame: Continuous check") {
    cv::Mat mat(100, 100, CV_8UC3);
    client::Frame frame(mat);

    CHECK(frame.Continuous());
  }

  TEST_CASE("Frame: Mat access") {
    cv::Mat mat(50, 50, CV_8UC1, cv::Scalar(42));
    client::Frame frame(mat);

    CHECK_EQ(frame.Mat().at<uint8_t>(0, 0), 42);

    // Modify via Mat reference
    frame.Mat().at<uint8_t>(0, 0) = 100;
    CHECK_EQ(frame.Mat().at<uint8_t>(0, 0), 100);

    // Const version
    const client::Frame& const_frame = frame;
    CHECK_EQ(const_frame.Mat().at<uint8_t>(0, 0), 100);
  }

  TEST_CASE("Frame: Self-assignment is safe") {
    cv::Mat mat(100, 100, CV_8UC3, cv::Scalar(1, 2, 3));
    client::Frame frame(mat);

    frame = frame;

    CHECK_FALSE(frame.Empty());
    CHECK_EQ(frame.Width(), 100);
    CHECK_EQ(frame.Height(), 100);
  }

  TEST_CASE("Frame: Empty frame operations") {
    client::Frame empty;

    CHECK(empty.Empty());
    CHECK(empty.Data().empty());
    CHECK_EQ(empty.TotalPixels(), 0);
  }
}
