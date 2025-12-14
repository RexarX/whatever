#include <doctest/doctest.h>

#include <client/app/face_data.hpp>

#include <cmath>

TEST_SUITE("client::Point2D") {
  TEST_CASE("Point2D: Default construction") {
    client::Point2D point;

    CHECK_EQ(point.x, doctest::Approx(0.0f));
    CHECK_EQ(point.y, doctest::Approx(0.0f));
  }

  TEST_CASE("Point2D: Aggregate initialization") {
    client::Point2D point{10.0f, 20.0f};

    CHECK_EQ(point.x, doctest::Approx(10.0f));
    CHECK_EQ(point.y, doctest::Approx(20.0f));
  }

  TEST_CASE("Point2D: DistanceTo calculates Euclidean distance") {
    client::Point2D p1{0.0f, 0.0f};
    client::Point2D p2{3.0f, 4.0f};

    CHECK_EQ(p1.DistanceTo(p2), doctest::Approx(5.0f));
    CHECK_EQ(p2.DistanceTo(p1), doctest::Approx(5.0f));
  }

  TEST_CASE("Point2D: DistanceTo same point is zero") {
    client::Point2D point{5.0f, 5.0f};

    CHECK_EQ(point.DistanceTo(point), doctest::Approx(0.0f));
  }

  TEST_CASE("Point2D: Equality operator") {
    client::Point2D p1{1.0f, 2.0f};
    client::Point2D p2{1.0f, 2.0f};
    client::Point2D p3{1.0f, 3.0f};

    CHECK(p1 == p2);
    CHECK_FALSE(p1 == p3);
  }

  TEST_CASE("Point2D: Addition operator") {
    client::Point2D p1{1.0f, 2.0f};
    client::Point2D p2{3.0f, 4.0f};

    auto result = p1 + p2;

    CHECK_EQ(result.x, doctest::Approx(4.0f));
    CHECK_EQ(result.y, doctest::Approx(6.0f));
  }

  TEST_CASE("Point2D: Subtraction operator") {
    client::Point2D p1{5.0f, 7.0f};
    client::Point2D p2{2.0f, 3.0f};

    auto result = p1 - p2;

    CHECK_EQ(result.x, doctest::Approx(3.0f));
    CHECK_EQ(result.y, doctest::Approx(4.0f));
  }

  TEST_CASE("Point2D: Multiplication operator") {
    client::Point2D point{2.0f, 3.0f};

    auto result = point * 2.0f;

    CHECK_EQ(result.x, doctest::Approx(4.0f));
    CHECK_EQ(result.y, doctest::Approx(6.0f));
  }

  TEST_CASE("Point2D: Division operator") {
    client::Point2D point{6.0f, 9.0f};

    auto result = point / 3.0f;

    CHECK_EQ(result.x, doctest::Approx(2.0f));
    CHECK_EQ(result.y, doctest::Approx(3.0f));
  }
}

TEST_SUITE("client::BoundingBox") {
  TEST_CASE("BoundingBox: Default construction") {
    client::BoundingBox box;

    CHECK_EQ(box.x, doctest::Approx(0.0f));
    CHECK_EQ(box.y, doctest::Approx(0.0f));
    CHECK_EQ(box.width, doctest::Approx(0.0f));
    CHECK_EQ(box.height, doctest::Approx(0.0f));
  }

  TEST_CASE("BoundingBox: Aggregate initialization") {
    client::BoundingBox box{10.0f, 20.0f, 100.0f, 50.0f};

    CHECK_EQ(box.x, doctest::Approx(10.0f));
    CHECK_EQ(box.y, doctest::Approx(20.0f));
    CHECK_EQ(box.width, doctest::Approx(100.0f));
    CHECK_EQ(box.height, doctest::Approx(50.0f));
  }

  TEST_CASE("BoundingBox: Center calculation") {
    client::BoundingBox box{0.0f, 0.0f, 100.0f, 100.0f};

    auto center = box.Center();

    CHECK_EQ(center.x, doctest::Approx(50.0f));
    CHECK_EQ(center.y, doctest::Approx(50.0f));
  }

  TEST_CASE("BoundingBox: Center with offset") {
    client::BoundingBox box{10.0f, 20.0f, 100.0f, 50.0f};

    auto center = box.Center();

    CHECK_EQ(center.x, doctest::Approx(60.0f));
    CHECK_EQ(center.y, doctest::Approx(45.0f));
  }

  TEST_CASE("BoundingBox: TopLeft") {
    client::BoundingBox box{15.0f, 25.0f, 100.0f, 50.0f};

    auto top_left = box.TopLeft();

    CHECK_EQ(top_left.x, doctest::Approx(15.0f));
    CHECK_EQ(top_left.y, doctest::Approx(25.0f));
  }

  TEST_CASE("BoundingBox: BottomRight") {
    client::BoundingBox box{10.0f, 20.0f, 100.0f, 50.0f};

    auto bottom_right = box.BottomRight();

    CHECK_EQ(bottom_right.x, doctest::Approx(110.0f));
    CHECK_EQ(bottom_right.y, doctest::Approx(70.0f));
  }

  TEST_CASE("BoundingBox: Area calculation") {
    client::BoundingBox box{0.0f, 0.0f, 10.0f, 20.0f};

    CHECK_EQ(box.Area(), doctest::Approx(200.0f));
  }

  TEST_CASE("BoundingBox: Valid check") {
    client::BoundingBox valid{0.0f, 0.0f, 10.0f, 20.0f};
    client::BoundingBox zero_width{0.0f, 0.0f, 0.0f, 20.0f};
    client::BoundingBox zero_height{0.0f, 0.0f, 10.0f, 0.0f};
    client::BoundingBox negative{0.0f, 0.0f, -10.0f, 20.0f};

    CHECK(valid.Valid());
    CHECK_FALSE(zero_width.Valid());
    CHECK_FALSE(zero_height.Valid());
    CHECK_FALSE(negative.Valid());
  }

  TEST_CASE("BoundingBox: Contains point inside") {
    client::BoundingBox box{0.0f, 0.0f, 100.0f, 100.0f};

    CHECK(box.Contains({50.0f, 50.0f}));
    CHECK(box.Contains({0.0f, 0.0f}));
    CHECK(box.Contains({100.0f, 100.0f}));
  }

  TEST_CASE("BoundingBox: Contains point outside") {
    client::BoundingBox box{10.0f, 10.0f, 100.0f, 100.0f};

    CHECK_FALSE(box.Contains({0.0f, 0.0f}));
    CHECK_FALSE(box.Contains({5.0f, 50.0f}));
    CHECK_FALSE(box.Contains({50.0f, 5.0f}));
    CHECK_FALSE(box.Contains({200.0f, 50.0f}));
  }

  TEST_CASE("BoundingBox: IoU with identical box") {
    client::BoundingBox box{0.0f, 0.0f, 100.0f, 100.0f};

    CHECK_EQ(box.IoU(box), doctest::Approx(1.0f));
  }

  TEST_CASE("BoundingBox: IoU with non-overlapping boxes") {
    client::BoundingBox box1{0.0f, 0.0f, 50.0f, 50.0f};
    client::BoundingBox box2{100.0f, 100.0f, 50.0f, 50.0f};

    CHECK_EQ(box1.IoU(box2), doctest::Approx(0.0f));
  }

  TEST_CASE("BoundingBox: IoU with partial overlap") {
    client::BoundingBox box1{0.0f, 0.0f, 100.0f, 100.0f};
    client::BoundingBox box2{50.0f, 50.0f, 100.0f, 100.0f};

    // Intersection: 50x50 = 2500
    // Union: 100*100 + 100*100 - 2500 = 17500
    // IoU: 2500/17500 ≈ 0.1429
    CHECK_EQ(box1.IoU(box2), doctest::Approx(2500.0f / 17500.0f));
  }

  TEST_CASE("BoundingBox: IoU is symmetric") {
    client::BoundingBox box1{0.0f, 0.0f, 100.0f, 100.0f};
    client::BoundingBox box2{25.0f, 25.0f, 100.0f, 100.0f};

    CHECK_EQ(box1.IoU(box2), doctest::Approx(box2.IoU(box1)));
  }

  TEST_CASE("BoundingBox: Equality operator") {
    client::BoundingBox box1{10.0f, 20.0f, 30.0f, 40.0f};
    client::BoundingBox box2{10.0f, 20.0f, 30.0f, 40.0f};
    client::BoundingBox box3{10.0f, 20.0f, 30.0f, 50.0f};

    CHECK(box1 == box2);
    CHECK_FALSE(box1 == box3);
  }
}

TEST_SUITE("client::FaceData") {
  TEST_CASE("FaceData: Default construction") {
    client::FaceData face;

    CHECK_EQ(face.confidence, doctest::Approx(0.0f));
    CHECK_EQ(face.relative_distance, doctest::Approx(0.0f));
    CHECK_EQ(face.track_id, 0u);
    CHECK_FALSE(face.Valid());
  }

  TEST_CASE("FaceData: Valid face") {
    client::FaceData face;
    face.bounding_box = {10.0f, 20.0f, 100.0f, 100.0f};
    face.confidence = 0.95f;
    face.track_id = 1;

    CHECK(face.Valid());
  }

  TEST_CASE("FaceData: Invalid with zero confidence") {
    client::FaceData face;
    face.bounding_box = {10.0f, 20.0f, 100.0f, 100.0f};
    face.confidence = 0.0f;

    CHECK_FALSE(face.Valid());
  }

  TEST_CASE("FaceData: Invalid with invalid bounding box") {
    client::FaceData face;
    face.bounding_box = {10.0f, 20.0f, 0.0f, 100.0f};
    face.confidence = 0.95f;

    CHECK_FALSE(face.Valid());
  }

  TEST_CASE("FaceData: Center returns bounding box center") {
    client::FaceData face;
    face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};

    auto center = face.Center();

    CHECK_EQ(center.x, doctest::Approx(50.0f));
    CHECK_EQ(center.y, doctest::Approx(50.0f));
  }

  TEST_CASE("FaceData: relative_distance field") {
    client::FaceData face;
    face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};
    face.confidence = 0.9f;
    face.relative_distance = 0.75f;

    CHECK_EQ(face.relative_distance, doctest::Approx(0.75f));
  }

  TEST_CASE("FaceData: CalculateRelativeDistance with small face") {
    client::FaceData face;
    // Small face: 50x50 = 2500 pixels
    face.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face.confidence = 0.9f;

    // Frame: 640x480 = 307200 pixels
    // Ratio: 2500 / 307200 ≈ 0.00814
    // Normalized: 0.00814 / 0.25 ≈ 0.0326
    float distance = face.CalculateRelativeDistance(640, 480);
    CHECK_LT(distance, 0.1f);  // Small face = far away
    CHECK_GE(distance, 0.0f);
  }

  TEST_CASE("FaceData: CalculateRelativeDistance with large face") {
    client::FaceData face;
    // Large face: 320x240 = 76800 pixels (25% of frame)
    face.bounding_box = {0.0f, 0.0f, 320.0f, 240.0f};
    face.confidence = 0.9f;

    // Frame: 640x480 = 307200 pixels
    // Ratio: 76800 / 307200 = 0.25
    // Normalized: 0.25 / 0.25 = 1.0
    float distance = face.CalculateRelativeDistance(640, 480);
    CHECK_EQ(distance, doctest::Approx(1.0f));  // Large face = very close
  }

  TEST_CASE("FaceData: CalculateRelativeDistance caps at 1.0") {
    client::FaceData face;
    // Very large face: larger than 25% of frame
    face.bounding_box = {0.0f, 0.0f, 400.0f, 300.0f};
    face.confidence = 0.9f;

    float distance = face.CalculateRelativeDistance(640, 480);
    CHECK_EQ(distance, doctest::Approx(1.0f));  // Capped at 1.0
  }

  TEST_CASE("FaceData: CalculateRelativeDistance with zero frame size") {
    client::FaceData face;
    face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};
    face.confidence = 0.9f;

    float distance_zero_width = face.CalculateRelativeDistance(0, 480);
    float distance_zero_height = face.CalculateRelativeDistance(640, 0);
    float distance_both_zero = face.CalculateRelativeDistance(0, 0);

    CHECK_EQ(distance_zero_width, doctest::Approx(0.0f));
    CHECK_EQ(distance_zero_height, doctest::Approx(0.0f));
    CHECK_EQ(distance_both_zero, doctest::Approx(0.0f));
  }

  TEST_CASE("FaceData: Priority calculation") {
    client::FaceData face;
    face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};
    face.confidence = 0.8f;
    face.relative_distance = 0.6f;

    // Default weights: distance=0.6, confidence=0.4
    // Priority = 0.6 * 0.6 + 0.8 * 0.4 = 0.36 + 0.32 = 0.68
    float priority = face.Priority();
    CHECK_EQ(priority, doctest::Approx(0.68f));
  }

  TEST_CASE("FaceData: Priority with custom weights") {
    client::FaceData face;
    face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};
    face.confidence = 0.8f;
    face.relative_distance = 0.6f;

    // Custom weights: distance=0.3, confidence=0.7
    // Priority = 0.6 * 0.3 + 0.8 * 0.7 = 0.18 + 0.56 = 0.74
    float priority = face.Priority(0.3f, 0.7f);
    CHECK_EQ(priority, doctest::Approx(0.74f));
  }

  TEST_CASE("FaceData: Priority comparison - close low confidence vs far high confidence") {
    client::FaceData close_face;
    close_face.bounding_box = {0.0f, 0.0f, 100.0f, 100.0f};
    close_face.confidence = 0.5f;
    close_face.relative_distance = 0.9f;

    client::FaceData far_face;
    far_face.bounding_box = {200.0f, 200.0f, 50.0f, 50.0f};
    far_face.confidence = 0.95f;
    far_face.relative_distance = 0.2f;

    // With default weights (distance=0.6, confidence=0.4):
    // Close: 0.9 * 0.6 + 0.5 * 0.4 = 0.54 + 0.2 = 0.74
    // Far:   0.2 * 0.6 + 0.95 * 0.4 = 0.12 + 0.38 = 0.50
    CHECK_GT(close_face.Priority(), far_face.Priority());
  }

  TEST_CASE("FaceData: Equality operator") {
    client::FaceData face1;
    face1.bounding_box = {10.0f, 20.0f, 30.0f, 40.0f};
    face1.confidence = 0.9f;
    face1.relative_distance = 0.5f;
    face1.track_id = 1;

    client::FaceData face2 = face1;

    client::FaceData face3;
    face3.bounding_box = {10.0f, 20.0f, 30.0f, 40.0f};
    face3.confidence = 0.8f;
    face3.relative_distance = 0.5f;
    face3.track_id = 1;

    CHECK(face1 == face2);
    CHECK_FALSE(face1 == face3);
  }

  TEST_CASE("FaceData: Equality with different relative_distance") {
    client::FaceData face1;
    face1.bounding_box = {10.0f, 20.0f, 30.0f, 40.0f};
    face1.confidence = 0.9f;
    face1.relative_distance = 0.5f;
    face1.track_id = 1;

    client::FaceData face2 = face1;
    face2.relative_distance = 0.8f;

    CHECK_FALSE(face1 == face2);
  }
}

TEST_SUITE("client::FaceDetectionResult") {
  TEST_CASE("FaceDetectionResult: Default construction") {
    client::FaceDetectionResult result;

    CHECK_FALSE(result.HasFaces());
    CHECK_EQ(result.FaceCount(), 0u);
    CHECK_EQ(result.frame_id, 0u);
    CHECK_EQ(result.processing_time_ms, doctest::Approx(0.0f));
  }

  TEST_CASE("FaceDetectionResult: HasFaces with faces") {
    client::FaceDetectionResult result;
    result.faces.push_back({});

    CHECK(result.HasFaces());
    CHECK_EQ(result.FaceCount(), 1u);
  }

  TEST_CASE("FaceDetectionResult: MostConfidentFace with no faces") {
    client::FaceDetectionResult result;

    auto most_confident = result.MostConfidentFace();

    CHECK_FALSE(most_confident.has_value());
  }

  TEST_CASE("FaceDetectionResult: MostConfidentFace with multiple faces") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face1.confidence = 0.7f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 50.0f, 50.0f};
    face2.confidence = 0.95f;
    face2.track_id = 2;

    client::FaceData face3;
    face3.bounding_box = {200.0f, 200.0f, 50.0f, 50.0f};
    face3.confidence = 0.8f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    auto most_confident = result.MostConfidentFace();

    CHECK(most_confident.has_value());
    CHECK_EQ(most_confident->confidence, doctest::Approx(0.95f));
    CHECK_EQ(most_confident->track_id, 2u);
  }

  TEST_CASE("FaceDetectionResult: LargestFace with no faces") {
    client::FaceDetectionResult result;

    auto largest = result.LargestFace();

    CHECK_FALSE(largest.has_value());
  }

  TEST_CASE("FaceDetectionResult: LargestFace with multiple faces") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};  // Area: 2500
    face1.confidence = 0.9f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 100.0f, 100.0f};  // Area: 10000
    face2.confidence = 0.7f;
    face2.track_id = 2;

    client::FaceData face3;
    face3.bounding_box = {200.0f, 200.0f, 75.0f, 75.0f};  // Area: 5625
    face3.confidence = 0.8f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    auto largest = result.LargestFace();

    CHECK(largest.has_value());
    CHECK_EQ(largest->bounding_box.Area(), doctest::Approx(10000.0f));
    CHECK_EQ(largest->track_id, 2u);
  }

  TEST_CASE("FaceDetectionResult: Single face is both most confident and largest") {
    client::FaceDetectionResult result;

    client::FaceData face;
    face.bounding_box = {50.0f, 50.0f, 100.0f, 100.0f};
    face.confidence = 0.85f;
    face.track_id = 42;

    result.faces = {face};

    auto most_confident = result.MostConfidentFace();
    auto largest = result.LargestFace();

    CHECK(most_confident.has_value());
    CHECK(largest.has_value());
    CHECK_EQ(most_confident->track_id, largest->track_id);
    CHECK_EQ(most_confident->track_id, 42u);
  }

  TEST_CASE("FaceDetectionResult: Frame ID and processing time") {
    client::FaceDetectionResult result;
    result.frame_id = 123;
    result.processing_time_ms = 15.5f;

    CHECK_EQ(result.frame_id, 123u);
    CHECK_EQ(result.processing_time_ms, doctest::Approx(15.5f));
  }

  TEST_CASE("FaceDetectionResult: ClosestFace with no faces") {
    client::FaceDetectionResult result;

    auto closest = result.ClosestFace();

    CHECK_FALSE(closest.has_value());
  }

  TEST_CASE("FaceDetectionResult: ClosestFace with multiple faces") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face1.confidence = 0.9f;
    face1.relative_distance = 0.3f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 100.0f, 100.0f};
    face2.confidence = 0.7f;
    face2.relative_distance = 0.9f;  // Closest
    face2.track_id = 2;

    client::FaceData face3;
    face3.bounding_box = {200.0f, 200.0f, 75.0f, 75.0f};
    face3.confidence = 0.8f;
    face3.relative_distance = 0.5f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    auto closest = result.ClosestFace();

    CHECK(closest.has_value());
    CHECK_EQ(closest->relative_distance, doctest::Approx(0.9f));
    CHECK_EQ(closest->track_id, 2u);
  }

  TEST_CASE("FaceDetectionResult: HighestPriorityFace with no faces") {
    client::FaceDetectionResult result;

    auto highest = result.HighestPriorityFace();

    CHECK_FALSE(highest.has_value());
  }

  TEST_CASE("FaceDetectionResult: HighestPriorityFace with multiple faces") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face1.confidence = 0.5f;
    face1.relative_distance = 0.9f;  // Close but low confidence
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 100.0f, 100.0f};
    face2.confidence = 0.95f;
    face2.relative_distance = 0.1f;  // Far but high confidence
    face2.track_id = 2;

    client::FaceData face3;
    face3.bounding_box = {200.0f, 200.0f, 75.0f, 75.0f};
    face3.confidence = 0.85f;
    face3.relative_distance = 0.7f;  // Good balance - should win
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    // Default weights: distance=0.6, confidence=0.4
    // face1: 0.9 * 0.6 + 0.5 * 0.4 = 0.54 + 0.20 = 0.74
    // face2: 0.1 * 0.6 + 0.95 * 0.4 = 0.06 + 0.38 = 0.44
    // face3: 0.7 * 0.6 + 0.85 * 0.4 = 0.42 + 0.34 = 0.76 (winner)

    auto highest = result.HighestPriorityFace();

    CHECK(highest.has_value());
    CHECK_EQ(highest->track_id, 3u);
  }

  TEST_CASE("FaceDetectionResult: HighestPriorityFace with custom weights") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face1.confidence = 0.95f;
    face1.relative_distance = 0.2f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 100.0f, 100.0f};
    face2.confidence = 0.6f;
    face2.relative_distance = 0.9f;
    face2.track_id = 2;

    result.faces = {face1, face2};

    // With confidence-heavy weights (distance=0.2, confidence=0.8)
    // face1: 0.2 * 0.2 + 0.95 * 0.8 = 0.04 + 0.76 = 0.80 (winner)
    // face2: 0.9 * 0.2 + 0.6 * 0.8 = 0.18 + 0.48 = 0.66

    auto highest = result.HighestPriorityFace(0.2f, 0.8f);

    CHECK(highest.has_value());
    CHECK_EQ(highest->track_id, 1u);
  }

  TEST_CASE("FaceDetectionResult: SortByPriority") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.bounding_box = {0.0f, 0.0f, 50.0f, 50.0f};
    face1.confidence = 0.5f;
    face1.relative_distance = 0.3f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.bounding_box = {100.0f, 100.0f, 100.0f, 100.0f};
    face2.confidence = 0.9f;
    face2.relative_distance = 0.9f;
    face2.track_id = 2;

    client::FaceData face3;
    face3.bounding_box = {200.0f, 200.0f, 75.0f, 75.0f};
    face3.confidence = 0.7f;
    face3.relative_distance = 0.6f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    result.SortByPriority();

    // face2 should be first (highest priority)
    CHECK_EQ(result.faces[0].track_id, 2u);
    // Verify descending order
    for (size_t i = 1; i < result.faces.size(); ++i) {
      CHECK_GE(result.faces[i - 1].Priority(), result.faces[i].Priority());
    }
  }

  TEST_CASE("FaceDetectionResult: SortByDistance") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.relative_distance = 0.3f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.relative_distance = 0.9f;
    face2.track_id = 2;

    client::FaceData face3;
    face3.relative_distance = 0.6f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    result.SortByDistance();

    // Closest first
    CHECK_EQ(result.faces[0].track_id, 2u);
    CHECK_EQ(result.faces[1].track_id, 3u);
    CHECK_EQ(result.faces[2].track_id, 1u);
  }

  TEST_CASE("FaceDetectionResult: SortByConfidence") {
    client::FaceDetectionResult result;

    client::FaceData face1;
    face1.confidence = 0.5f;
    face1.track_id = 1;

    client::FaceData face2;
    face2.confidence = 0.95f;
    face2.track_id = 2;

    client::FaceData face3;
    face3.confidence = 0.7f;
    face3.track_id = 3;

    result.faces = {face1, face2, face3};

    result.SortByConfidence();

    // Highest confidence first
    CHECK_EQ(result.faces[0].track_id, 2u);
    CHECK_EQ(result.faces[1].track_id, 3u);
    CHECK_EQ(result.faces[2].track_id, 1u);
  }

  TEST_CASE("FaceDetectionResult: Sort with empty faces list") {
    client::FaceDetectionResult result;

    CHECK_NOTHROW(result.SortByPriority());
    CHECK_NOTHROW(result.SortByDistance());
    CHECK_NOTHROW(result.SortByConfidence());
    CHECK(result.faces.empty());
  }

  TEST_CASE("FaceDetectionResult: Sort with single face") {
    client::FaceDetectionResult result;

    client::FaceData face;
    face.confidence = 0.9f;
    face.relative_distance = 0.5f;
    face.track_id = 1;

    result.faces = {face};

    CHECK_NOTHROW(result.SortByPriority());
    CHECK_EQ(result.faces.size(), 1u);
    CHECK_EQ(result.faces[0].track_id, 1u);
  }
}
