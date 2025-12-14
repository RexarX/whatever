#pragma once

#include <client/pch.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace client {

/**
 * @brief 2D point structure for face landmarks and positions.
 */
struct Point2D {
  float x = 0.0F;  ///< X coordinate.
  float y = 0.0F;  ///< Y coordinate.

  /**
   * @brief Calculates the Euclidean distance to another point.
   * @param other The other point.
   * @return Distance between points.
   */
  [[nodiscard]] float DistanceTo(Point2D other) const noexcept;

  [[nodiscard]] constexpr bool operator==(const Point2D& other) const noexcept = default;

  [[nodiscard]] constexpr Point2D operator+(Point2D other) const noexcept { return {x + other.x, y + other.y}; }

  [[nodiscard]] constexpr Point2D operator-(Point2D other) const noexcept { return {x - other.x, y - other.y}; }

  [[nodiscard]] constexpr Point2D operator*(float scalar) const noexcept { return {x * scalar, y * scalar}; }

  [[nodiscard]] constexpr Point2D operator/(float scalar) const noexcept { return {x / scalar, y / scalar}; }
};

inline float Point2D::DistanceTo(Point2D other) const noexcept {
  const float dx = x - other.x;
  const float dy = y - other.y;
  return std::sqrt(dx * dx + dy * dy);
}

/**
 * @brief Axis-aligned bounding box for face detection.
 */
struct BoundingBox {
  float x = 0.0F;       ///< Top-left X coordinate.
  float y = 0.0F;       ///< Top-left Y coordinate.
  float width = 0.0F;   ///< Box width.
  float height = 0.0F;  ///< Box height.

  /**
   * @brief Gets the center point of the bounding box.
   * @return Center point.
   */
  [[nodiscard]] constexpr Point2D Center() const noexcept { return {.x = x + width / 2.0F, .y = y + height / 2.0F}; }

  /**
   * @brief Gets the top-left corner of the bounding box.
   * @return Top-left point.
   */
  [[nodiscard]] constexpr Point2D TopLeft() const noexcept { return {.x = x, .y = y}; }

  /**
   * @brief Gets the bottom-right corner of the bounding box.
   * @return Bottom-right point.
   */
  [[nodiscard]] constexpr Point2D BottomRight() const noexcept { return {.x = x + width, .y = y + height}; }

  /**
   * @brief Calculates the area of the bounding box.
   * @return Area in square pixels.
   */
  [[nodiscard]] constexpr float Area() const noexcept { return width * height; }

  /**
   * @brief Checks if the bounding box is valid (non-zero dimensions).
   * @return True if valid.
   */
  [[nodiscard]] constexpr bool Valid() const noexcept { return width > 0.0F && height > 0.0F; }

  /**
   * @brief Checks if a point is inside the bounding box.
   * @param point The point to check.
   * @return True if the point is inside.
   */
  [[nodiscard]] constexpr bool Contains(Point2D point) const noexcept {
    return point.x >= x && point.x <= x + width && point.y >= y && point.y <= y + height;
  }

  /**
   * @brief Calculates IoU (Intersection over Union) with another box.
   * @param other The other bounding box.
   * @return IoU value between 0 and 1.
   */
  [[nodiscard]] constexpr float IoU(const BoundingBox& other) const noexcept;

  [[nodiscard]] constexpr bool operator==(const BoundingBox& other) const noexcept = default;
};

constexpr float BoundingBox::IoU(const BoundingBox& other) const noexcept {
  const float inter_x1 = std::max(x, other.x);
  const float inter_y1 = std::max(y, other.y);
  const float inter_x2 = std::min(x + width, other.x + other.width);
  const float inter_y2 = std::min(y + height, other.y + other.height);

  if (inter_x2 <= inter_x1 || inter_y2 <= inter_y1) {
    return 0.0F;
  }

  const float inter_area = (inter_x2 - inter_x1) * (inter_y2 - inter_y1);
  const float union_area = Area() + other.Area() - inter_area;

  return union_area > 0.0F ? inter_area / union_area : 0.0F;
}

/**
 * @brief Data structure containing face detection results.
 */
struct FaceData {
  BoundingBox bounding_box;        ///< Face bounding box in image coordinates.
  float confidence = 0.0F;         ///< Detection confidence score (0.0 - 1.0).
  float relative_distance = 0.0F;  ///< Relative distance estimate (0.0 = far, 1.0 = close).
  uint32_t track_id = 0;           ///< Tracking ID for temporal consistency.

  /**
   * @brief Checks if the face detection is valid.
   * @return True if the detection has a valid bounding box and confidence.
   */
  [[nodiscard]] constexpr bool Valid() const noexcept { return bounding_box.Valid() && confidence > 0.0f; }

  /**
   * @brief Gets the center of the detected face.
   * @return Face center point.
   */
  [[nodiscard]] constexpr Point2D Center() const noexcept { return bounding_box.Center(); }

  /**
   * @brief Calculates relative distance from bounding box area.
   * @details Larger faces are assumed to be closer. This normalizes the area
   * relative to the frame dimensions to produce a value between 0 and 1.
   * @param frame_width Frame width in pixels.
   * @param frame_height Frame height in pixels.
   * @return Relative distance (0.0 = far, 1.0 = close/fills frame).
   */
  [[nodiscard]] constexpr float CalculateRelativeDistance(int frame_width, int frame_height) const noexcept {
    if (frame_width <= 0 || frame_height <= 0) {
      return 0.0F;
    }
    const float frame_area = static_cast<float>(frame_width * frame_height);
    const float face_area = bounding_box.Area();
    // Normalize: a face taking up ~25% of the frame is considered "very close" (1.0)
    // This provides a reasonable scale for most use cases
    constexpr float kMaxExpectedRatio = 0.25F;
    const float ratio = face_area / frame_area;
    return std::min(ratio / kMaxExpectedRatio, 1.0F);
  }

  /**
   * @brief Calculates priority score for face selection.
   * @details Combines relative distance and confidence to produce a priority score.
   * Closer faces with higher confidence get higher priority.
   * @param distance_weight Weight for distance component (default 0.6).
   * @param confidence_weight Weight for confidence component (default 0.4).
   * @return Priority score (higher = more priority).
   */
  [[nodiscard]] constexpr float Priority(float distance_weight = 0.6F, float confidence_weight = 0.4F) const noexcept {
    return relative_distance * distance_weight + confidence * confidence_weight;
  }

  [[nodiscard]] constexpr bool operator==(const FaceData& other) const noexcept = default;
};

/**
 * @brief Container for multiple face detections in a single frame.
 */
struct FaceDetectionResult {
  std::vector<FaceData> faces;      ///< Detected faces.
  uint64_t frame_id = 0;            ///< Frame identifier for tracking.
  float processing_time_ms = 0.0F;  ///< Time taken to process the frame.

  /**
   * @brief Checks if any faces were detected.
   * @return True if at least one face was detected.
   */
  [[nodiscard]] constexpr bool HasFaces() const noexcept { return !faces.empty(); }

  /**
   * @brief Gets the number of detected faces.
   * @return Face count.
   */
  [[nodiscard]] constexpr size_t FaceCount() const noexcept { return faces.size(); }

  /**
   * @brief Gets the face with the highest confidence.
   * @return The most confident face detection, or nullopt if no faces.
   */
  [[nodiscard]] constexpr auto MostConfidentFace() const noexcept -> std::optional<FaceData>;

  /**
   * @brief Gets the largest face by bounding box area.
   * @return The largest face detection, or nullopt if no faces.
   */
  [[nodiscard]] constexpr auto LargestFace() const noexcept -> std::optional<FaceData>;

  /**
   * @brief Gets the closest face by relative distance.
   * @return The closest face detection, or nullopt if no faces.
   */
  [[nodiscard]] constexpr auto ClosestFace() const noexcept -> std::optional<FaceData>;

  /**
   * @brief Gets the face with highest priority (combining distance and confidence).
   * @param distance_weight Weight for distance component (default 0.6).
   * @param confidence_weight Weight for confidence component (default 0.4).
   * @return The highest priority face, or nullopt if no faces.
   */
  [[nodiscard]] constexpr auto HighestPriorityFace(float distance_weight = 0.6F,
                                                   float confidence_weight = 0.4F) const noexcept
      -> std::optional<FaceData>;

  /**
   * @brief Sorts faces by priority (closest and most confident first).
   * @details Modifies the faces vector in-place, sorting by priority score descending.
   * @param distance_weight Weight for distance component (default 0.6).
   * @param confidence_weight Weight for confidence component (default 0.4).
   */
  void SortByPriority(float distance_weight = 0.6F, float confidence_weight = 0.4F);

  /**
   * @brief Sorts faces by relative distance (closest first).
   */
  void SortByDistance() {
    std::ranges::sort(
        faces, [](const FaceData& lhs, const FaceData& rhs) { return lhs.relative_distance > rhs.relative_distance; });
  }

  /**
   * @brief Sorts faces by confidence (highest first).
   */
  void SortByConfidence() {
    std::ranges::sort(faces, [](const FaceData& lhs, const FaceData& rhs) { return lhs.confidence > rhs.confidence; });
  }
};

constexpr auto FaceDetectionResult::MostConfidentFace() const noexcept -> std::optional<FaceData> {
  if (faces.empty()) {
    return std::nullopt;
  }

  auto best = std::cref(faces.front());
  for (const auto& face : faces) {
    if (face.confidence > best.get().confidence) {
      best = face;
    }
  }
  return best;
}

constexpr auto FaceDetectionResult::LargestFace() const noexcept -> std::optional<FaceData> {
  if (faces.empty()) {
    return std::nullopt;
  }

  auto largest = std::cref(faces.front());
  for (const auto& face : faces) {
    if (face.bounding_box.Area() > largest.get().bounding_box.Area()) {
      largest = face;
    }
  }
  return largest;
}

constexpr auto FaceDetectionResult::ClosestFace() const noexcept -> std::optional<FaceData> {
  if (faces.empty()) {
    return std::nullopt;
  }

  auto closest = std::cref(faces.front());
  for (const auto& face : faces) {
    if (face.relative_distance > closest.get().relative_distance) {
      closest = face;
    }
  }
  return closest;
}

constexpr auto FaceDetectionResult::HighestPriorityFace(float distance_weight, float confidence_weight) const noexcept
    -> std::optional<FaceData> {
  if (faces.empty()) {
    return std::nullopt;
  }

  auto highest = std::cref(faces.front());
  float highest_priority = highest.get().Priority(distance_weight, confidence_weight);

  for (const auto& face : faces) {
    const float priority = face.Priority(distance_weight, confidence_weight);
    if (priority > highest_priority) {
      highest = face;
      highest_priority = priority;
    }
  }
  return highest;
}

inline void FaceDetectionResult::SortByPriority(float distance_weight, float confidence_weight) {
  std::ranges::sort(faces, [distance_weight, confidence_weight](const FaceData& a, const FaceData& b) {
    return a.Priority(distance_weight, confidence_weight) > b.Priority(distance_weight, confidence_weight);
  });
}

}  // namespace client
