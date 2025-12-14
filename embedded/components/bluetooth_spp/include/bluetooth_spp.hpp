#pragma once

#include <esp_err.h>
#include <esp_gap_bt_api.h>
#include <esp_spp_api.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace embedded {

/**
 * @brief Bluetooth SPP connection state.
 */
enum class BluetoothState : uint8_t {
  kUninitialized = 0,  ///< Not initialized.
  kInitialized,        ///< Initialized but not connected.
  kConnecting,         ///< Connection in progress.
  kConnected,          ///< Connected to a client.
  kDisconnecting,      ///< Disconnection in progress.
  kError               ///< Error state.
};

// Forward declarations for callback functions
void SppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param);
void GapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param);

/**
 * @brief Bluetooth SPP manager for ESP32.
 * @details Provides Bluetooth Serial Port Profile (SPP) server functionality
 * for receiving commands from the client application.
 */
class BluetoothSpp {
public:
  /**
   * @brief Maximum size of a received data packet.
   */
  static constexpr size_t kMaxPacketSize = 512;

/**
 * @brief Callback type for connection state changes.
 */
#if __cpp_lib_move_only_function >= 202110L
  using StateCallback = std::move_only_function<void(BluetoothState state)>;
#else
  using StateCallback = std::function<void(BluetoothState state)>;
#endif

/**
 * @brief Callback type for data received.
 */
#if __cpp_lib_move_only_function >= 202110L
  using DataCallback = std::move_only_function<void(std::span<const uint8_t> data)>;
#else
  using DataCallback = std::function<void(std::span<const uint8_t> data)>;
#endif

  BluetoothSpp();
  BluetoothSpp(const BluetoothSpp&) = delete;
  BluetoothSpp(BluetoothSpp&&) = delete;
  ~BluetoothSpp();

  BluetoothSpp& operator=(const BluetoothSpp&) = delete;
  BluetoothSpp& operator=(BluetoothSpp&&) = delete;

  /**
   * @brief Initializes Bluetooth SPP server.
   * @param device_name Device name visible during discovery
   * @return ESP_OK on success, error code on failure
   */
  esp_err_t Initialize(std::string_view device_name = "ESP32-FaceTracker");

  /**
   * @brief Deinitializes Bluetooth SPP server.
   * @return ESP_OK on success, error code on failure
   */
  esp_err_t Deinitialize();

  /**
   * @brief Sends data to the connected client.
   * @param data Data to send
   * @return Number of bytes sent, or negative error code
   */
  int Send(std::span<const uint8_t> data);

  /**
   * @brief Sets the state change callback.
   * @param callback Callback to invoke on state changes
   */
  void SetStateCallback(StateCallback callback) noexcept { state_callback_ = std::move(callback); }

  /**
   * @brief Sets the data received callback.
   * @param callback Callback to invoke when data is received
   */
  void SetDataCallback(DataCallback callback) noexcept { data_callback_ = std::move(callback); }

  /**
   * @brief Handles SPP events from the Bluetooth stack.
   * @param event SPP event type
   * @param param Event parameters
   */
  void HandleSppEvent(uint32_t event, void* param);

  /**
   * @brief Handles GAP events from the Bluetooth stack.
   * @param event GAP event type
   * @param param Event parameters
   */
  void HandleGapEvent(uint32_t event, void* param);

  /**
   * @brief Checks if Bluetooth is initialized.
   * @return True if initialized
   */
  [[nodiscard]] bool Initialized() const noexcept { return state_ != BluetoothState::kUninitialized; }

  /**
   * @brief Checks if a client is connected.
   * @return True if connected
   */
  [[nodiscard]] bool Connected() const noexcept { return state_ == BluetoothState::kConnected; }

  /**
   * @brief Gets the current connection state.
   * @return Current Bluetooth state
   */
  [[nodiscard]] BluetoothState State() const noexcept { return state_; }

  /**
   * @brief Gets the singleton instance.
   * @return Reference to the BluetoothSpp instance
   */
  [[nodiscard]] static BluetoothSpp& Instance() {
    static BluetoothSpp instance;
    return instance;
  }

private:
  void SetState(BluetoothState state);

  BluetoothState state_ = BluetoothState::kUninitialized;
  uint32_t connection_handle_ = 0;
  StateCallback state_callback_;
  DataCallback data_callback_;
};

}  // namespace embedded
