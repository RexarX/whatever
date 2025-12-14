#pragma once

#include <client/comm/pch.hpp>

#include <client/comm/export.hpp>
#include <client/comm/protocol.hpp>
#include <client/core/utils/fast_pimpl.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace client::comm {

/**
 * @brief Bluetooth connection state.
 */
enum class BluetoothState : uint8_t {
  kDisconnected = 0,  ///< Not connected to any device.
  kScanning,          ///< Scanning for devices.
  kConnecting,        ///< Attempting to connect.
  kConnected,         ///< Connected to a device.
  kError              ///< Error state.
};

/**
 * @brief Error codes for Bluetooth operations.
 */
enum class BluetoothError : uint8_t {
  kOk = 0,            ///< Operation succeeded.
  kNotSupported,      ///< Bluetooth not supported on this platform.
  kNotEnabled,        ///< Bluetooth is disabled.
  kDeviceNotFound,    ///< Device not found.
  kConnectionFailed,  ///< Failed to connect to device.
  kConnectionLost,    ///< Connection was lost.
  kSendFailed,        ///< Failed to send data.
  kReceiveFailed,     ///< Failed to receive data.
  kTimeout,           ///< Operation timed out.
  kAlreadyConnected,  ///< Already connected to a device.
  kNotConnected,      ///< Not connected to any device.
  kInternalError      ///< Internal error.
};

/**
 * @brief Converts BluetoothError to a human-readable string.
 * @param error The error to convert
 * @return A string view representing the error
 */
[[nodiscard]] constexpr std::string_view BluetoothErrorToString(BluetoothError error) noexcept {
  switch (error) {
    case BluetoothError::kOk:
      return "OK";
    case BluetoothError::kNotSupported:
      return "Bluetooth not supported";
    case BluetoothError::kNotEnabled:
      return "Bluetooth is disabled";
    case BluetoothError::kDeviceNotFound:
      return "Device not found";
    case BluetoothError::kConnectionFailed:
      return "Connection failed";
    case BluetoothError::kConnectionLost:
      return "Connection lost";
    case BluetoothError::kSendFailed:
      return "Failed to send data";
    case BluetoothError::kReceiveFailed:
      return "Failed to receive data";
    case BluetoothError::kTimeout:
      return "Operation timed out";
    case BluetoothError::kAlreadyConnected:
      return "Already connected";
    case BluetoothError::kNotConnected:
      return "Not connected";
    case BluetoothError::kInternalError:
      return "Internal error";
    default:
      return "Unknown error";
  }
}

/**
 * @brief Converts BluetoothState to a human-readable string.
 * @param state The state to convert
 * @return A string view representing the state
 */
[[nodiscard]] constexpr std::string_view BluetoothStateToString(BluetoothState state) noexcept {
  switch (state) {
    case BluetoothState::kDisconnected:
      return "Disconnected";
    case BluetoothState::kScanning:
      return "Scanning";
    case BluetoothState::kConnecting:
      return "Connecting";
    case BluetoothState::kConnected:
      return "Connected";
    case BluetoothState::kError:
      return "Error";
    default:
      return "Unknown";
  }
}

/**
 * @brief Information about a discovered Bluetooth device.
 */
struct CLIENT_COMM_API BluetoothDevice {
  std::string name;           ///< Device name.
  std::string address;        ///< Device address (MAC address or UUID).
  int16_t rssi = 0;           ///< Signal strength in dBm.
  bool is_paired = false;     ///< Whether device is paired.
  bool is_connected = false;  ///< Whether device is currently connected.

  [[nodiscard]] bool operator==(const BluetoothDevice&) const noexcept = default;
};

/**
 * @brief Bluetooth manager for handling device discovery and connections.
 * @details Provides a platform-agnostic interface for Bluetooth operations
 * using Qt Bluetooth when available.
 * @note Uses unique_ptr for pimpl since the implementation contains QObject-derived
 * types which are not moveable.
 */
class CLIENT_COMM_API BluetoothManager {
public:
  /**
   * @brief Callback type for state changes.
   */
#if __cpp_lib_move_only_function >= 202110L
  using StateCallback = std::move_only_function<void(BluetoothState state, std::string_view error_message)>;
#else
  using StateCallback = std::function<void(BluetoothState state, std::string_view error_message)>;
#endif

  /**
   * @brief Callback type for device discovery.
   */
#if __cpp_lib_move_only_function >= 202110L
  using DeviceDiscoveredCallback = std::move_only_function<void(const BluetoothDevice& device)>;
#else
  using DeviceDiscoveredCallback = std::function<void(const BluetoothDevice& device)>;
#endif

  /**
   * @brief Callback type for scan completion.
   */
#if __cpp_lib_move_only_function >= 202110L
  using ScanCompleteCallback = std::move_only_function<void(std::span<const BluetoothDevice> devices)>;
#else
  using ScanCompleteCallback = std::function<void(std::span<const BluetoothDevice> devices)>;
#endif

  /**
   * @brief Callback type for data received.
   */
#if __cpp_lib_move_only_function >= 202110L
  using DataReceivedCallback = std::move_only_function<void(std::span<const uint8_t> data)>;
#else
  using DataReceivedCallback = std::function<void(std::span<const uint8_t> data)>;
#endif

  BluetoothManager();
  BluetoothManager(const BluetoothManager&) = delete;
  BluetoothManager(BluetoothManager&&) = delete;
  ~BluetoothManager();

  BluetoothManager& operator=(const BluetoothManager&) = delete;
  BluetoothManager& operator=(BluetoothManager&&) = delete;

  /**
   * @brief Initializes the Bluetooth manager.
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto Initialize() -> std::expected<void, BluetoothError>;

  /**
   * @brief Starts scanning for Bluetooth devices.
   * @param timeout_ms Scan timeout in milliseconds (0 for default)
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto StartScan(uint32_t timeout_ms = 10000) -> std::expected<void, BluetoothError>;

  /**
   * @brief Stops scanning for Bluetooth devices.
   */
  void StopScan();

  /**
   * @brief Connects to a Bluetooth device.
   * @param address Device address to connect to
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto Connect(std::string_view address) -> std::expected<void, BluetoothError>;

  /**
   * @brief Disconnects from the current device.
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto Disconnect() -> std::expected<void, BluetoothError>;

  /**
   * @brief Sends data to the connected device.
   * @param data Data to send
   * @return Expected number of bytes sent, or error on failure
   */
  [[nodiscard]] auto Send(std::span<const uint8_t> data) -> std::expected<size_t, BluetoothError>;

  /**
   * @brief Sends a protocol message to the connected device.
   * @param cmd Servo command to send
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto SendCommand(const ServoCommand& cmd) -> std::expected<void, BluetoothError>;

  /**
   * @brief Sends a heartbeat message to the connected device.
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto SendHeartbeat() -> std::expected<void, BluetoothError>;

  /**
   * @brief Sends a calibrate command to the connected device.
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto SendCalibrate() -> std::expected<void, BluetoothError>;

  /**
   * @brief Sends a home command to the connected device.
   * @return Expected void on success, or error on failure
   */
  [[nodiscard]] auto SendHome() -> std::expected<void, BluetoothError>;

  /**
   * @brief Sets the state change callback.
   * @param callback Callback to invoke on state changes
   */
  void SetStateCallback(StateCallback callback) noexcept;

  /**
   * @brief Sets the device discovered callback.
   * @param callback Callback to invoke when a device is discovered
   */
  void SetDeviceDiscoveredCallback(DeviceDiscoveredCallback callback) noexcept;

  /**
   * @brief Sets the scan complete callback.
   * @param callback Callback to invoke when scan completes
   */
  void SetScanCompleteCallback(ScanCompleteCallback callback) noexcept;

  /**
   * @brief Sets the data received callback.
   * @param callback Callback to invoke when data is received
   */
  void SetDataReceivedCallback(DataReceivedCallback callback) noexcept;

  /**
   * @brief Processes pending Bluetooth events.
   * @details Qt events are processed via the Qt event loop automatically.
   */
  void ProcessEvents() noexcept {}

  /**
   * @brief Checks if Bluetooth is available on this platform.
   * @return True if Bluetooth is available
   */
  [[nodiscard]] bool Available() const noexcept;

  /**
   * @brief Checks if Bluetooth is enabled.
   * @return True if Bluetooth is enabled
   */
  [[nodiscard]] bool Enabled() const noexcept;

  /**
   * @brief Gets the list of discovered devices.
   * @return List of discovered devices
   */
  [[nodiscard]] auto DiscoveredDevices() const -> std::vector<BluetoothDevice>;

  /**
   * @brief Gets the current connection state.
   * @return Current Bluetooth state
   */
  [[nodiscard]] BluetoothState State() const noexcept;

  /**
   * @brief Gets the currently connected device info.
   * @return Connected device info, or nullopt if not connected
   */
  [[nodiscard]] auto ConnectedDevice() const -> std::optional<BluetoothDevice>;

  /**
   * @brief Gets the last error message.
   * @return Last error message, or empty string if no error
   */
  [[nodiscard]] std::string_view LastError() const noexcept;

  /**
   * @brief Gets the protocol handler.
   * @return Reference to the protocol handler
   */
  [[nodiscard]] Protocol& GetProtocol() noexcept;

  /**
   * @brief Gets the protocol handler.
   * @return Const reference to the protocol handler
   */
  [[nodiscard]] const Protocol& GetProtocol() const noexcept;

private:
  static constexpr size_t kImplSize = 416;
  static constexpr size_t kImplAlign = 8;

  struct Impl;
  utils::FastPimpl<Impl, kImplSize, kImplAlign> impl_;
};

}  // namespace client::comm
