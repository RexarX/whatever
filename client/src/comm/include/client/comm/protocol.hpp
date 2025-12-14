#pragma once

#include <client/comm/pch.hpp>

#include <client/comm/export.hpp>
#include <client/core/utils/fast_pimpl.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace client::comm {

/**
 * @brief Error codes for protocol operations.
 */
enum class ProtocolError : uint8_t {
  kOk = 0,                 ///< Operation succeeded.
  kSerializationFailed,    ///< Failed to serialize message.
  kDeserializationFailed,  ///< Failed to deserialize message.
  kInvalidMessage,         ///< Message validation failed.
  kBufferTooSmall,         ///< Output buffer is too small.
  kUnknownMessageType,     ///< Unknown message type encountered.
};

/**
 * @brief Converts ProtocolError to a human-readable string.
 * @param error The error to convert
 * @return A string view representing the error
 */
[[nodiscard]] constexpr std::string_view ProtocolErrorToString(ProtocolError error) noexcept {
  switch (error) {
    case ProtocolError::kOk:
      return "OK";
    case ProtocolError::kSerializationFailed:
      return "Serialization failed";
    case ProtocolError::kDeserializationFailed:
      return "Deserialization failed";
    case ProtocolError::kInvalidMessage:
      return "Invalid message";
    case ProtocolError::kBufferTooSmall:
      return "Buffer too small";
    case ProtocolError::kUnknownMessageType:
      return "Unknown message type";
    default:
      return "Unknown error";
  }
}

/**
 * @brief Message types supported by the protocol.
 */
enum class MessageType : uint8_t {
  kUnknown = 0,
  kServoCommand,  ///< Command to move servos.
  kFaceData,      ///< Face detection data.
  kCalibration,   ///< Calibration data.
  kStatus,        ///< Status message.
  kHeartbeat,     ///< Heartbeat/keep-alive message.
  kConfig,        ///< Configuration message.
};

/**
 * @brief Servo command data.
 * @details Contains pan and tilt angles for servo control.
 */
struct CLIENT_COMM_API ServoCommand {
  float pan_angle = 0.0F;   ///< Pan angle in degrees (-90 to 90).
  float tilt_angle = 0.0F;  ///< Tilt angle in degrees (-45 to 45).
  float speed = 1.0F;       ///< Movement speed multiplier (0.0 to 1.0).
  bool smooth = true;       ///< Use smooth interpolated movement.

  [[nodiscard]] bool operator==(const ServoCommand&) const noexcept = default;
};

/**
 * @brief Face position data for tracking.
 */
struct CLIENT_COMM_API FacePosition {
  float x = 0.0F;           ///< Normalized X position (0.0 to 1.0, center = 0.5).
  float y = 0.0F;           ///< Normalized Y position (0.0 to 1.0, center = 0.5).
  float width = 0.0F;       ///< Normalized face width (0.0 to 1.0).
  float height = 0.0F;      ///< Normalized face height (0.0 to 1.0).
  float confidence = 0.0F;  ///< Detection confidence (0.0 to 1.0).
  int32_t track_id = -1;    ///< Tracking ID (-1 if not tracked).

  [[nodiscard]] bool operator==(const FacePosition&) const noexcept = default;
};

/**
 * @brief Face data message containing detected faces.
 */
struct CLIENT_COMM_API FaceDataMessage {
  std::vector<FacePosition> faces;  ///< List of detected faces.
  uint64_t timestamp_ms = 0;        ///< Timestamp in milliseconds.
  uint32_t frame_id = 0;            ///< Frame identifier.

  [[nodiscard]] bool operator==(const FaceDataMessage&) const noexcept = default;
};

/**
 * @brief Status message from the device.
 */
struct CLIENT_COMM_API StatusMessage {
  float pan_position = 0.0F;   ///< Current pan position in degrees.
  float tilt_position = 0.0F;  ///< Current tilt position in degrees.
  float battery_level = 1.0F;  ///< Battery level (0.0 to 1.0).
  bool is_calibrated = false;  ///< Whether the device is calibrated.
  bool is_tracking = false;    ///< Whether tracking is active.
  uint32_t error_code = 0;     ///< Error code (0 = no error).

  [[nodiscard]] bool operator==(const StatusMessage&) const noexcept = default;
};

/**
 * @brief Heartbeat message for connection keep-alive.
 */
struct CLIENT_COMM_API HeartbeatMessage {
  uint64_t timestamp_ms = 0;  ///< Timestamp in milliseconds.
  uint32_t sequence = 0;      ///< Sequence number.

  [[nodiscard]] bool operator==(const HeartbeatMessage&) const noexcept = default;
};

/**
 * @brief Protocol handler for serializing and deserializing messages.
 * @details This class wraps protobuf serialization/deserialization to isolate
 * protobuf symbols from other parts of the application (specifically OpenCV).
 */
class CLIENT_COMM_API Protocol {
public:
  constexpr Protocol() noexcept = default;
  constexpr Protocol(const Protocol&) = default;
  constexpr Protocol(Protocol&&) = default;
  constexpr ~Protocol() = default;

  constexpr Protocol& operator=(const Protocol&) = default;
  constexpr Protocol& operator=(Protocol&&) = default;

  /**
   * @brief Serializes a ServoCommand to bytes.
   * @param cmd The command to serialize
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeServoCommand(const ServoCommand& cmd)
      -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Deserializes a ServoCommand from bytes.
   * @param data The serialized data
   * @return Deserialized command or error
   */
  [[nodiscard]] static auto DeserializeServoCommand(std::span<const uint8_t> data)
      -> std::expected<ServoCommand, ProtocolError>;

  /**
   * @brief Serializes a FaceDataMessage to bytes.
   * @param msg The message to serialize
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeFaceData(const FaceDataMessage& msg)
      -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Deserializes a FaceDataMessage from bytes.
   * @param data The serialized data
   * @return Deserialized message or error
   */
  [[nodiscard]] static auto DeserializeFaceData(std::span<const uint8_t> data)
      -> std::expected<FaceDataMessage, ProtocolError>;

  /**
   * @brief Serializes a StatusMessage to bytes.
   * @param msg The message to serialize
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeStatus(const StatusMessage& msg)
      -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Deserializes a StatusMessage from bytes.
   * @param data The serialized data
   * @return Deserialized message or error
   */
  [[nodiscard]] static auto DeserializeStatus(std::span<const uint8_t> data)
      -> std::expected<StatusMessage, ProtocolError>;

  /**
   * @brief Serializes a HeartbeatMessage to bytes.
   * @param msg The message to serialize
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeHeartbeat(const HeartbeatMessage& msg)
      -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Deserializes a HeartbeatMessage from bytes.
   * @param data The serialized data
   * @return Deserialized message or error
   */
  [[nodiscard]] static auto DeserializeHeartbeat(std::span<const uint8_t> data)
      -> std::expected<HeartbeatMessage, ProtocolError>;

  /**
   * @brief Serializes a calibrate command to bytes.
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeCalibrate() -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Serializes a home command to bytes.
   * @return Serialized bytes or error
   */
  [[nodiscard]] static auto SerializeHome() -> std::expected<std::vector<uint8_t>, ProtocolError>;

  /**
   * @brief Detects the message type from serialized data.
   * @param data The serialized data
   * @return Detected message type or unknown
   */
  [[nodiscard]] static auto DetectMessageType(std::span<const uint8_t> data) -> MessageType;

  /**
   * @brief Gets the current protocol version.
   * @return Protocol version string
   */
  [[nodiscard]] static constexpr std::string_view Version() noexcept { return "1.0.0"; }
};

}  // namespace client::comm
