#include <doctest/doctest.h>

#include <client/comm/protocol.hpp>

#include <cstdint>
#include <vector>

TEST_SUITE("client::comm::Protocol") {
  TEST_CASE("ProtocolError: ProtocolErrorToString returns correct strings") {
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kOk), "OK");
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kSerializationFailed),
             "Serialization failed");
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kDeserializationFailed),
             "Deserialization failed");
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kInvalidMessage), "Invalid message");
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kBufferTooSmall), "Buffer too small");
    CHECK_EQ(client::comm::ProtocolErrorToString(client::comm::ProtocolError::kUnknownMessageType),
             "Unknown message type");
  }

  TEST_CASE("ProtocolError: Error codes are distinct") {
    CHECK_NE(client::comm::ProtocolError::kOk, client::comm::ProtocolError::kSerializationFailed);
    CHECK_NE(client::comm::ProtocolError::kSerializationFailed, client::comm::ProtocolError::kDeserializationFailed);
    CHECK_NE(client::comm::ProtocolError::kDeserializationFailed, client::comm::ProtocolError::kInvalidMessage);
    CHECK_NE(client::comm::ProtocolError::kInvalidMessage, client::comm::ProtocolError::kBufferTooSmall);
    CHECK_NE(client::comm::ProtocolError::kBufferTooSmall, client::comm::ProtocolError::kUnknownMessageType);
  }

  TEST_CASE("ServoCommand: Default construction") {
    client::comm::ServoCommand cmd;
    CHECK_EQ(cmd.pan_angle, doctest::Approx(0.0));
    CHECK_EQ(cmd.tilt_angle, doctest::Approx(0.0));
    CHECK_EQ(cmd.speed, doctest::Approx(1.0));
    CHECK(cmd.smooth);
  }

  TEST_CASE("ServoCommand: Equality operator") {
    client::comm::ServoCommand cmd1{.pan_angle = 45.0F, .tilt_angle = -30.0F, .speed = 0.5F, .smooth = false};
    client::comm::ServoCommand cmd2{.pan_angle = 45.0F, .tilt_angle = -30.0F, .speed = 0.5F, .smooth = false};
    client::comm::ServoCommand cmd3{.pan_angle = 45.0F, .tilt_angle = -30.0F, .speed = 0.5F, .smooth = true};

    CHECK(cmd1 == cmd2);
    CHECK_FALSE(cmd1 == cmd3);
  }

  TEST_CASE("FacePosition: Default construction") {
    client::comm::FacePosition pos;
    CHECK_EQ(pos.x, doctest::Approx(0.0));
    CHECK_EQ(pos.y, doctest::Approx(0.0));
    CHECK_EQ(pos.width, doctest::Approx(0.0));
    CHECK_EQ(pos.height, doctest::Approx(0.0));
    CHECK_EQ(pos.confidence, doctest::Approx(0.0));
    CHECK_EQ(pos.track_id, -1);
  }

  TEST_CASE("FacePosition: Equality operator") {
    client::comm::FacePosition pos1{
        .x = 0.5F, .y = 0.5F, .width = 0.2F, .height = 0.3F, .confidence = 0.9F, .track_id = 1};
    client::comm::FacePosition pos2{
        .x = 0.5F, .y = 0.5F, .width = 0.2F, .height = 0.3F, .confidence = 0.9F, .track_id = 1};
    client::comm::FacePosition pos3{
        .x = 0.5F, .y = 0.5F, .width = 0.2F, .height = 0.3F, .confidence = 0.9F, .track_id = 2};

    CHECK(pos1 == pos2);
    CHECK_FALSE(pos1 == pos3);
  }

  TEST_CASE("FaceDataMessage: Default construction") {
    client::comm::FaceDataMessage msg;
    CHECK(msg.faces.empty());
    CHECK_EQ(msg.timestamp_ms, 0U);
    CHECK_EQ(msg.frame_id, 0U);
  }

  TEST_CASE("FaceDataMessage: With faces") {
    client::comm::FaceDataMessage msg;
    msg.faces.push_back({.x = 0.3F, .y = 0.4F, .width = 0.1F, .height = 0.15F, .confidence = 0.85F, .track_id = 0});
    msg.faces.push_back({.x = 0.7F, .y = 0.6F, .width = 0.12F, .height = 0.18F, .confidence = 0.92F, .track_id = 1});
    msg.timestamp_ms = 1234567890;
    msg.frame_id = 42;

    CHECK_EQ(msg.faces.size(), 2U);
    CHECK_EQ(msg.timestamp_ms, 1234567890U);
    CHECK_EQ(msg.frame_id, 42U);
  }

  TEST_CASE("StatusMessage: Default construction") {
    client::comm::StatusMessage msg;
    CHECK_EQ(msg.pan_position, doctest::Approx(0.0));
    CHECK_EQ(msg.tilt_position, doctest::Approx(0.0));
    CHECK_EQ(msg.battery_level, doctest::Approx(1.0));
    CHECK_FALSE(msg.is_calibrated);
    CHECK_FALSE(msg.is_tracking);
    CHECK_EQ(msg.error_code, 0U);
  }

  TEST_CASE("StatusMessage: Equality operator") {
    client::comm::StatusMessage msg1{.pan_position = 30.0F,
                                     .tilt_position = -15.0F,
                                     .battery_level = 0.75F,
                                     .is_calibrated = true,
                                     .is_tracking = true,
                                     .error_code = 0};
    client::comm::StatusMessage msg2{.pan_position = 30.0F,
                                     .tilt_position = -15.0F,
                                     .battery_level = 0.75F,
                                     .is_calibrated = true,
                                     .is_tracking = true,
                                     .error_code = 0};
    client::comm::StatusMessage msg3{.pan_position = 30.0F,
                                     .tilt_position = -15.0F,
                                     .battery_level = 0.75F,
                                     .is_calibrated = false,
                                     .is_tracking = true,
                                     .error_code = 0};

    CHECK(msg1 == msg2);
    CHECK_FALSE(msg1 == msg3);
  }

  TEST_CASE("HeartbeatMessage: Default construction") {
    client::comm::HeartbeatMessage msg;
    CHECK_EQ(msg.timestamp_ms, 0U);
    CHECK_EQ(msg.sequence, 0U);
  }

  TEST_CASE("HeartbeatMessage: Equality operator") {
    client::comm::HeartbeatMessage msg1{.timestamp_ms = 1000, .sequence = 5};
    client::comm::HeartbeatMessage msg2{.timestamp_ms = 1000, .sequence = 5};
    client::comm::HeartbeatMessage msg3{.timestamp_ms = 1000, .sequence = 6};

    CHECK(msg1 == msg2);
    CHECK_FALSE(msg1 == msg3);
  }

  TEST_CASE("Protocol: Default construction") {
    client::comm::Protocol protocol;
    CHECK_EQ(client::comm::Protocol::Version(), "1.0.0");
  }

  TEST_CASE("Protocol: Move construction") {
    client::comm::Protocol protocol1;
    client::comm::Protocol protocol2(std::move(protocol1));
    CHECK_EQ(client::comm::Protocol::Version(), "1.0.0");
  }

  TEST_CASE("Protocol: Move assignment") {
    client::comm::Protocol protocol1;
    client::comm::Protocol protocol2;
    protocol2 = std::move(protocol1);
    CHECK_EQ(client::comm::Protocol::Version(), "1.0.0");
  }

  TEST_CASE("Protocol: ServoCommand round-trip") {
    client::comm::Protocol protocol;
    client::comm::ServoCommand cmd{.pan_angle = 45.0F, .tilt_angle = -30.0F, .speed = 0.8F, .smooth = true};

    auto serialized = protocol.SerializeServoCommand(cmd);
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    auto deserialized = protocol.DeserializeServoCommand(*serialized);
    REQUIRE(deserialized.has_value());
    // Note: Only pan_angle and tilt_angle are serialized in the current protocol
    // speed and smooth are not part of the protobuf message and use defaults
    CHECK_EQ(deserialized->pan_angle, doctest::Approx(static_cast<double>(cmd.pan_angle)));
    CHECK_EQ(deserialized->tilt_angle, doctest::Approx(static_cast<double>(cmd.tilt_angle)));
    // speed defaults to 1.0 and smooth defaults to true in deserialization
    CHECK_EQ(deserialized->speed, doctest::Approx(1.0));
    CHECK(deserialized->smooth);
  }

  TEST_CASE("Protocol: FaceDataMessage round-trip") {
    client::comm::Protocol protocol;
    client::comm::FaceDataMessage msg;
    // Use center position (0.5, 0.5) for clean round-trip
    msg.faces.push_back({.x = 0.5F, .y = 0.5F, .width = 0.2F, .height = 0.25F, .confidence = 0.95F, .track_id = 1});
    msg.timestamp_ms = 9876543210;
    msg.frame_id = 100;

    auto serialized = protocol.SerializeFaceData(msg);
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    auto deserialized = protocol.DeserializeFaceData(*serialized);
    REQUIRE(deserialized.has_value());
    REQUIRE_EQ(deserialized->faces.size(), msg.faces.size());
    // Note: The protocol converts x,y through pan/tilt angles, so we check approximate equality
    CHECK_EQ(deserialized->faces[0].x, doctest::Approx(static_cast<double>(msg.faces[0].x)));
    CHECK_EQ(deserialized->faces[0].y, doctest::Approx(static_cast<double>(msg.faces[0].y)));
    // width, height, confidence, track_id are not preserved - they use defaults
    CHECK_EQ(deserialized->faces[0].width, doctest::Approx(0.1));
    CHECK_EQ(deserialized->faces[0].height, doctest::Approx(0.1));
    CHECK_EQ(deserialized->faces[0].confidence, doctest::Approx(1.0));
    CHECK_EQ(deserialized->faces[0].track_id, 0);
    CHECK_EQ(deserialized->timestamp_ms, msg.timestamp_ms);
    CHECK_EQ(deserialized->frame_id, msg.frame_id);
  }

  TEST_CASE("Protocol: FaceDataMessage with multiple faces round-trip") {
    client::comm::Protocol protocol;
    client::comm::FaceDataMessage msg;
    msg.faces.push_back({.x = 0.3F, .y = 0.3F, .width = 0.15F, .height = 0.2F, .confidence = 0.8F, .track_id = 0});
    msg.faces.push_back({.x = 0.7F, .y = 0.5F, .width = 0.18F, .height = 0.22F, .confidence = 0.9F, .track_id = 1});
    msg.faces.push_back({.x = 0.5F, .y = 0.7F, .width = 0.1F, .height = 0.12F, .confidence = 0.75F, .track_id = 2});
    msg.timestamp_ms = 12345;
    msg.frame_id = 50;

    auto serialized = protocol.SerializeFaceData(msg);
    REQUIRE(serialized.has_value());

    auto deserialized = protocol.DeserializeFaceData(*serialized);
    REQUIRE(deserialized.has_value());
    // Note: Current protocol implementation only preserves the first face
    CHECK_EQ(deserialized->faces.size(), 1U);
  }

  TEST_CASE("Protocol: StatusMessage round-trip") {
    client::comm::Protocol protocol;
    client::comm::StatusMessage msg{.pan_position = 60.0F,
                                    .tilt_position = -20.0F,
                                    .battery_level = 0.5F,
                                    .is_calibrated = true,
                                    .is_tracking = true,
                                    .error_code = 0};

    auto serialized = protocol.SerializeStatus(msg);
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    auto deserialized = protocol.DeserializeStatus(*serialized);
    REQUIRE(deserialized.has_value());
    CHECK_EQ(deserialized->pan_position, doctest::Approx(static_cast<double>(msg.pan_position)));
    CHECK_EQ(deserialized->tilt_position, doctest::Approx(static_cast<double>(msg.tilt_position)));
    // battery_level is not in the proto, defaults to 1.0
    CHECK_EQ(deserialized->battery_level, doctest::Approx(1.0));
    CHECK_EQ(deserialized->is_calibrated, msg.is_calibrated);
    CHECK_EQ(deserialized->is_tracking, msg.is_tracking);
    CHECK_EQ(deserialized->error_code, msg.error_code);
  }

  TEST_CASE("Protocol: HeartbeatMessage round-trip") {
    client::comm::Protocol protocol;
    client::comm::HeartbeatMessage msg{.timestamp_ms = 555666777, .sequence = 42};

    auto serialized = protocol.SerializeHeartbeat(msg);
    REQUIRE(serialized.has_value());
    CHECK_FALSE(serialized->empty());

    auto deserialized = protocol.DeserializeHeartbeat(*serialized);
    REQUIRE(deserialized.has_value());
    CHECK_EQ(deserialized->timestamp_ms, msg.timestamp_ms);
    CHECK_EQ(deserialized->sequence, msg.sequence);
  }

  TEST_CASE("Protocol: Deserialize empty data fails") {
    client::comm::Protocol protocol;
    std::vector<uint8_t> empty_data;

    auto result = protocol.DeserializeServoCommand(empty_data);
    CHECK_FALSE(result.has_value());
  }

  TEST_CASE("Protocol: Deserialize garbage data") {
    client::comm::Protocol protocol;
    std::vector<uint8_t> garbage{0xFF, 0xFE, 0xFD, 0xFC, 0xFB};

    // May or may not fail depending on protobuf parsing
    // We just check that it doesn't crash
    [[maybe_unused]] auto result = protocol.DeserializeServoCommand(garbage);
    CHECK(true);
  }

  TEST_CASE("Protocol: Empty FaceDataMessage round-trip") {
    client::comm::Protocol protocol;
    client::comm::FaceDataMessage msg;  // No faces

    auto serialized = protocol.SerializeFaceData(msg);
    REQUIRE(serialized.has_value());

    auto deserialized = protocol.DeserializeFaceData(*serialized);
    REQUIRE(deserialized.has_value());
    CHECK(deserialized->faces.empty());
    CHECK_EQ(deserialized->timestamp_ms, 0U);
    CHECK_EQ(deserialized->frame_id, 0U);
  }

  TEST_CASE("MessageType: Enum values are distinct") {
    CHECK_NE(client::comm::MessageType::kUnknown, client::comm::MessageType::kServoCommand);
    CHECK_NE(client::comm::MessageType::kServoCommand, client::comm::MessageType::kFaceData);
    CHECK_NE(client::comm::MessageType::kFaceData, client::comm::MessageType::kCalibration);
    CHECK_NE(client::comm::MessageType::kCalibration, client::comm::MessageType::kStatus);
    CHECK_NE(client::comm::MessageType::kStatus, client::comm::MessageType::kHeartbeat);
    CHECK_NE(client::comm::MessageType::kHeartbeat, client::comm::MessageType::kConfig);
  }

  TEST_CASE("Protocol::Version: Returns valid version string") {
    const auto version = client::comm::Protocol::Version();
    CHECK_FALSE(version.empty());
    CHECK_NE(version.find('.'), std::string_view::npos);  // Contains at least one dot
  }

}  // TEST_SUITE
