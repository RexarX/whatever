#pragma once

#include <client/pch.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

#include <opencv2/core/mat.hpp>

namespace client {

/**
 * @brief Wrapper around cv::Mat representing a video frame.
 * @details Provides a type-safe interface for video frame data with ownership semantics.
 * Supports both owning and non-owning views.
 */
class Frame {
public:
  /**
   * @brief Constructs an empty frame.
   */
  Frame() noexcept = default;

  /**
   * @brief Constructs a frame from a cv::Mat.
   * @param mat The OpenCV matrix to wrap (performs shallow copy by default).
   */
  explicit Frame(cv::Mat mat) noexcept : mat_(std::move(mat)) {}

  /**
   * @brief Constructs a frame with specified dimensions.
   * @param width Frame width in pixels.
   * @param height Frame height in pixels.
   * @param type OpenCV matrix type (e.g., CV_8UC3).
   */
  Frame(int width, int height, int type) : mat_(height, width, type) {}

  Frame(const Frame& other) : mat_(other.mat_.clone()) {}
  Frame(Frame&& other) noexcept : mat_(std::move(other.mat_)) {}
  ~Frame() noexcept = default;

  Frame& operator=(const Frame& other);
  Frame& operator=(Frame&& other) noexcept;

  /**
   * @brief Creates a deep copy of the frame.
   * @return New Frame with copied data.
   */
  [[nodiscard]] Frame Clone() const;

  /**
   * @brief Converts the frame to a different color space.
   * @param code OpenCV color conversion code (e.g., cv::COLOR_BGR2RGB).
   * @return New Frame in the target color space.
   */
  [[nodiscard]] Frame ConvertColor(int code) const;

  /**
   * @brief Resizes the frame.
   * @param width Target width.
   * @param height Target height.
   * @return New resized Frame.
   */
  [[nodiscard]] Frame Resize(int width, int height) const;

  /**
   * @brief Checks if the frame data is continuous in memory.
   * @return True if data is continuous.
   */
  [[nodiscard]] bool Continuous() const noexcept { return mat_.isContinuous(); }

  /**
   * @brief Checks if the frame contains valid data.
   * @return True if the frame is empty.
   */
  [[nodiscard]] bool Empty() const noexcept { return mat_.empty(); }

  /**
   * @brief Explicit conversion to bool for empty checking.
   * @return True if the frame is not empty.
   */
  explicit operator bool() const noexcept { return !Empty(); }

  /**
   * @brief Gets the frame width.
   * @return Width in pixels.
   */
  [[nodiscard]] int Width() const noexcept { return mat_.cols; }

  /**
   * @brief Gets the frame height.
   * @return Height in pixels.
   */
  [[nodiscard]] int Height() const noexcept { return mat_.rows; }

  /**
   * @brief Gets the number of channels.
   * @return Number of color channels.
   */
  [[nodiscard]] int Channels() const noexcept { return mat_.channels(); }

  /**
   * @brief Gets the total number of pixels.
   * @return Total pixel count.
   */
  [[nodiscard]] size_t TotalPixels() const noexcept { return mat_.total(); }

  /**
   * @brief Gets the OpenCV matrix type.
   * @return Matrix type (e.g., CV_8UC3).
   */
  [[nodiscard]] int Type() const noexcept { return mat_.type(); }

  /**
   * @brief Gets the step (stride) in bytes between rows.
   * @return Row stride in bytes.
   */
  [[nodiscard]] size_t Step() const noexcept { return mat_.step; }

  /**
   * @brief Gets read-only span of pixel data.
   * @return Span providing read-only access to pixel data.
   */
  [[nodiscard]] auto Data() const noexcept -> std::span<const uint8_t>;

  /**
   * @brief Gets the underlying cv::Mat.
   * @return Reference to the internal matrix.
   */
  [[nodiscard]] cv::Mat& Mat() noexcept { return mat_; }

  /**
   * @brief Gets the underlying cv::Mat (const version).
   * @return Const reference to the internal matrix.
   */
  [[nodiscard]] const cv::Mat& Mat() const noexcept { return mat_; }

private:
  cv::Mat mat_;  ///< Internal OpenCV matrix.
};

inline Frame& Frame::operator=(const Frame& other) {
  if (this != &other) {
    mat_ = other.mat_.clone();
  }
  return *this;
}

inline Frame& Frame::operator=(Frame&& other) noexcept {
  if (this != &other) {
    mat_ = std::move(other.mat_);
  }
  return *this;
}

inline auto Frame::Data() const noexcept -> std::span<const uint8_t> {
  if (mat_.empty()) {
    return {};
  }
  return {mat_.data, mat_.total() * mat_.elemSize()};
}

}  // namespace client
