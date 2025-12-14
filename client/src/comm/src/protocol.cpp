#include <client/comm/protocol.hpp>

#include "messages.pb.h"

#include <google/protobuf/message.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace client::comm {

auto Protocol::SerializeServoCommand(const ServoCommand& cmd) -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    app::Command proto_cmd;
    proto_cmd.set_type(app::COMMAND_TYPE_MOVE);

    auto* move = proto_cmd.mutable_move();
    auto* target = move->mutable_target_position();
    target->set_pan(cmd.pan_angle);
    target->set_tilt(cmd.tilt_angle);
    move->set_use_face_tracking(false);

    const size_t size = proto_cmd.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_cmd.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::DeserializeServoCommand(std::span<const uint8_t> data) -> std::expected<ServoCommand, ProtocolError> {
  try {
    app::Command proto_cmd;
    if (!proto_cmd.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return std::unexpected(ProtocolError::kDeserializationFailed);
    }

    if (proto_cmd.type() != app::COMMAND_TYPE_MOVE || !proto_cmd.has_move()) {
      return std::unexpected(ProtocolError::kInvalidMessage);
    }

    const auto& move = proto_cmd.move();
    const auto& target = move.target_position();

    ServoCommand cmd;
    cmd.pan_angle = target.pan();
    cmd.tilt_angle = target.tilt();
    cmd.speed = 1.0F;
    cmd.smooth = true;

    return cmd;
  } catch (...) {
    return std::unexpected(ProtocolError::kDeserializationFailed);
  }
}

auto Protocol::SerializeFaceData(const FaceDataMessage& msg) -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    // We'll use a Command with move type for face data
    // In a real implementation, you might want to add a dedicated message type
    app::Command proto_cmd;
    proto_cmd.set_id(msg.frame_id);
    proto_cmd.set_timestamp_ms(msg.timestamp_ms);
    proto_cmd.set_type(app::COMMAND_TYPE_MOVE);

    if (!msg.faces.empty()) {
      auto* move = proto_cmd.mutable_move();
      move->set_use_face_tracking(true);

      // Use the first face as the target
      const auto& face = msg.faces.front();
      auto* target = move->mutable_target_position();

      // Convert normalized position to pan/tilt angles
      // Center is at (0.5, 0.5), map to [-90, 90] for pan and [-45, 45] for tilt
      const float pan = (face.x - 0.5F) * 180.0F;
      const float tilt = (face.y - 0.5F) * 90.0F;
      target->set_pan(pan);
      target->set_tilt(tilt);
    }

    const size_t size = proto_cmd.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_cmd.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::DeserializeFaceData(std::span<const uint8_t> data) -> std::expected<FaceDataMessage, ProtocolError> {
  try {
    app::Command proto_cmd;
    if (!proto_cmd.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return std::unexpected(ProtocolError::kDeserializationFailed);
    }

    FaceDataMessage msg;
    msg.frame_id = proto_cmd.id();
    msg.timestamp_ms = proto_cmd.timestamp_ms();

    if (proto_cmd.has_move() && proto_cmd.move().use_face_tracking()) {
      const auto& target = proto_cmd.move().target_position();

      FacePosition face;
      // Convert pan/tilt back to normalized position
      face.x = (target.pan() / 180.0F) + 0.5F;
      face.y = (target.tilt() / 90.0F) + 0.5F;
      face.width = 0.1F;
      face.height = 0.1F;
      face.confidence = 1.0F;
      face.track_id = 0;

      msg.faces.push_back(face);
    }

    return msg;
  } catch (...) {
    return std::unexpected(ProtocolError::kDeserializationFailed);
  }
}

auto Protocol::SerializeStatus(const StatusMessage& msg) -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    app::Response proto_resp;
    proto_resp.set_status(msg.error_code == 0 ? app::STATUS_CODE_OK : app::STATUS_CODE_ERROR);

    auto* status = proto_resp.mutable_device_status();
    auto* current = status->mutable_current_position();
    current->set_pan(msg.pan_position);
    current->set_tilt(msg.tilt_position);
    status->set_is_calibrated(msg.is_calibrated);
    status->set_is_moving(msg.is_tracking);

    const size_t size = proto_resp.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_resp.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::DeserializeStatus(std::span<const uint8_t> data) -> std::expected<StatusMessage, ProtocolError> {
  try {
    app::Response proto_resp;
    if (!proto_resp.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return std::unexpected(ProtocolError::kDeserializationFailed);
    }

    StatusMessage msg;

    if (proto_resp.has_device_status()) {
      const auto& status = proto_resp.device_status();
      const auto& current = status.current_position();

      msg.pan_position = current.pan();
      msg.tilt_position = current.tilt();
      msg.is_calibrated = status.is_calibrated();
      msg.is_tracking = status.is_moving();
    }

    msg.error_code = proto_resp.status() == app::STATUS_CODE_OK ? 0 : static_cast<uint32_t>(proto_resp.status());
    msg.battery_level = 1.0F;  // Not in proto, default to full

    return msg;
  } catch (...) {
    return std::unexpected(ProtocolError::kDeserializationFailed);
  }
}

auto Protocol::SerializeHeartbeat(const HeartbeatMessage& msg) -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    app::Command proto_cmd;
    proto_cmd.set_id(msg.sequence);
    proto_cmd.set_timestamp_ms(msg.timestamp_ms);
    proto_cmd.set_type(app::COMMAND_TYPE_PING);

    const size_t size = proto_cmd.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_cmd.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::DeserializeHeartbeat(std::span<const uint8_t> data) -> std::expected<HeartbeatMessage, ProtocolError> {
  try {
    app::Command proto_cmd;
    if (!proto_cmd.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      return std::unexpected(ProtocolError::kDeserializationFailed);
    }

    if (proto_cmd.type() != app::COMMAND_TYPE_PING) {
      return std::unexpected(ProtocolError::kInvalidMessage);
    }

    HeartbeatMessage msg;
    msg.sequence = proto_cmd.id();
    msg.timestamp_ms = proto_cmd.timestamp_ms();

    return msg;
  } catch (...) {
    return std::unexpected(ProtocolError::kDeserializationFailed);
  }
}

auto Protocol::SerializeCalibrate() -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    app::Command proto_cmd;
    proto_cmd.set_type(app::COMMAND_TYPE_CALIBRATE);

    auto* calibrate = proto_cmd.mutable_calibrate();
    calibrate->set_mode(app::CalibrateCommand_Mode_MODE_FULL);

    const size_t size = proto_cmd.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_cmd.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::SerializeHome() -> std::expected<std::vector<uint8_t>, ProtocolError> {
  try {
    app::Command proto_cmd;
    proto_cmd.set_type(app::COMMAND_TYPE_HOME);

    const size_t size = proto_cmd.ByteSizeLong();
    std::vector<uint8_t> buffer(size);

    if (!proto_cmd.SerializeToArray(buffer.data(), static_cast<int>(size))) {
      return std::unexpected(ProtocolError::kSerializationFailed);
    }

    return buffer;
  } catch (...) {
    return std::unexpected(ProtocolError::kSerializationFailed);
  }
}

auto Protocol::DetectMessageType(std::span<const uint8_t> data) -> MessageType {
  // Try to parse as Command first
  {
    app::Command proto_cmd;
    if (proto_cmd.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      switch (proto_cmd.type()) {
        case app::COMMAND_TYPE_MOVE:
          if (proto_cmd.has_move() && proto_cmd.move().use_face_tracking()) {
            return MessageType::kFaceData;
          }
          return MessageType::kServoCommand;
        case app::COMMAND_TYPE_PING:
          return MessageType::kHeartbeat;
        case app::COMMAND_TYPE_CALIBRATE:
          return MessageType::kCalibration;
        case app::COMMAND_TYPE_SET_CONFIG:
          return MessageType::kConfig;
        default:
          break;
      }
    }
  }

  // Try to parse as Response
  {
    app::Response proto_resp;
    if (proto_resp.ParseFromArray(data.data(), static_cast<int>(data.size()))) {
      if (proto_resp.has_device_status()) {
        return MessageType::kStatus;
      }
    }
  }

  return MessageType::kUnknown;
}

}  // namespace client::comm
