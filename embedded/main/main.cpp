/**
 * ESP32 Firmware - Face Tracker Servo Controller
 *
 * Main application entry point for the servo controller.
 * Receives commands from the desktop/mobile client over Bluetooth SPP
 * and controls servos to track the user's face.
 *
 * Uses nanopb (lightweight protobuf) for message serialization.
 */

#include <bluetooth_spp.hpp>
#include <servo_controller.hpp>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <esp_log.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <nvs_flash.h>

// Nanopb protobuf headers
#include <messages.pb.h>
#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include <array>
#include <atomic>
#include <cstring>
#include <span>

namespace {

constexpr const char* kTag = "main";
constexpr const char* kDeviceName = "ESP32-FaceTracker";

// Global servo controller
embedded::ServoController g_servo_controller;

// Command queue for inter-task communication
QueueHandle_t g_command_queue = nullptr;
constexpr size_t kCommandQueueSize = 10;

// Buffer for received commands
struct CommandBuffer {
  std::array<uint8_t, 512> data;
  size_t length = 0;
};

// Forward declarations
void ProcessCommand(const app_Command& cmd);
void SendStatusResponse(uint32_t command_id);
void SendErrorResponse(uint32_t command_id, app_StatusCode status, const char* message);
void SendPingResponse(uint32_t command_id, uint64_t client_timestamp);
void OnBluetoothStateChanged(embedded::BluetoothState state);
void OnBluetoothDataReceived(std::span<const uint8_t> data);
void ServoTask(void* param);

/**
 * @brief Sends a status response to the client.
 */
void SendStatusResponse(uint32_t command_id) {
  auto& bt = embedded::BluetoothSpp::Instance();
  if (!bt.Connected()) {
    return;
  }

  const auto state = g_servo_controller.State();

  app_Response response = app_Response_init_zero;
  response.command_id = command_id;
  response.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  response.status = app_StatusCode_STATUS_CODE_OK;
  response.which_payload = app_Response_device_status_tag;

  auto& status = response.payload.device_status;
  status.has_current_position = true;
  status.current_position.pan = state.pan;
  status.current_position.tilt = state.tilt;
  status.has_target_position = true;
  status.target_position.pan = state.target_pan;
  status.target_position.tilt = state.target_tilt;
  status.is_calibrated = state.is_calibrated;
  status.is_moving = state.is_moving;
  status.uptime_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  status.free_heap = static_cast<uint32_t>(esp_get_free_heap_size());
  status.wifi_rssi = 0;  // Not using WiFi

  // Encode response
  std::array<uint8_t, 256> buffer;
  pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

  if (pb_encode(&stream, app_Response_fields, &response)) {
    bt.Send(std::span<const uint8_t>(buffer.data(), stream.bytes_written));
    ESP_LOGD(kTag, "Status response sent: %zu bytes", stream.bytes_written);
  } else {
    ESP_LOGE(kTag, "Failed to encode status response");
  }
}

/**
 * @brief Sends an error response to the client.
 */
void SendErrorResponse(uint32_t command_id, app_StatusCode status, const char* message) {
  auto& bt = embedded::BluetoothSpp::Instance();
  if (!bt.Connected()) {
    return;
  }

  app_Response response = app_Response_init_zero;
  response.command_id = command_id;
  response.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  response.status = status;
  response.which_payload = app_Response_error_tag;

  auto& error = response.payload.error;
  error.code = status;
  strncpy(error.message, message, sizeof(error.message) - 1);
  error.message[sizeof(error.message) - 1] = '\0';

  // Encode response
  std::array<uint8_t, 256> buffer;
  pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

  if (pb_encode(&stream, app_Response_fields, &response)) {
    bt.Send(std::span<const uint8_t>(buffer.data(), stream.bytes_written));
    ESP_LOGD(kTag, "Error response sent: %zu bytes", stream.bytes_written);
  } else {
    ESP_LOGE(kTag, "Failed to encode error response");
  }
}

/**
 * @brief Sends a ping response to the client.
 */
void SendPingResponse(uint32_t command_id, uint64_t /*client_timestamp*/) {
  auto& bt = embedded::BluetoothSpp::Instance();
  if (!bt.Connected()) {
    return;
  }

  app_Response response = app_Response_init_zero;
  response.command_id = command_id;
  response.timestamp_ms = static_cast<uint64_t>(esp_timer_get_time() / 1000);
  response.status = app_StatusCode_STATUS_CODE_OK;

  // Encode response
  std::array<uint8_t, 64> buffer;
  pb_ostream_t stream = pb_ostream_from_buffer(buffer.data(), buffer.size());

  if (pb_encode(&stream, app_Response_fields, &response)) {
    bt.Send(std::span<const uint8_t>(buffer.data(), stream.bytes_written));
    ESP_LOGD(kTag, "Ping response sent");
  } else {
    ESP_LOGE(kTag, "Failed to encode ping response");
  }
}

/**
 * @brief Processes a received Command message.
 */
void ProcessCommand(const app_Command& cmd) {
  ESP_LOGI(kTag, "Processing command: type=%d, id=%lu", cmd.type, static_cast<unsigned long>(cmd.id));

  switch (cmd.type) {
    case app_CommandType_COMMAND_TYPE_MOVE: {
      if (cmd.which_payload == app_Command_move_tag && cmd.payload.move.has_target_position) {
        const auto& target = cmd.payload.move.target_position;
        ESP_LOGI(kTag, "Move command: pan=%.2f, tilt=%.2f", static_cast<double>(target.pan),
                 static_cast<double>(target.tilt));

        // Check if calibrated
        if (!g_servo_controller.IsCalibrated()) {
          ESP_LOGW(kTag, "Servos not calibrated, rejecting move command");
          SendErrorResponse(cmd.id, app_StatusCode_STATUS_CODE_NOT_CALIBRATED, "Servos not calibrated");
          break;
        }

        // Move servos
        const bool use_smooth = !cmd.payload.move.use_face_tracking;  // Use smooth for direct commands
        g_servo_controller.MoveTo(target.pan, target.tilt, use_smooth);

        // Send success response
        SendStatusResponse(cmd.id);
      } else {
        ESP_LOGW(kTag, "Move command missing target position");
        SendErrorResponse(cmd.id, app_StatusCode_STATUS_CODE_INVALID_COMMAND, "Missing target position");
      }
      break;
    }

    case app_CommandType_COMMAND_TYPE_HOME: {
      ESP_LOGI(kTag, "Home command received");
      g_servo_controller.Home();
      SendStatusResponse(cmd.id);
      break;
    }

    case app_CommandType_COMMAND_TYPE_CALIBRATE: {
      ESP_LOGI(kTag, "Calibrate command received");

      // Get calibration mode
      app_CalibrateCommand_Mode mode = app_CalibrateCommand_Mode_MODE_FULL;
      if (cmd.which_payload == app_Command_calibrate_tag) {
        mode = cmd.payload.calibrate.mode;
      }

      ESP_LOGI(kTag, "Calibration mode: %d", mode);

      // Perform calibration
      g_servo_controller.Calibrate();

      SendStatusResponse(cmd.id);
      break;
    }

    case app_CommandType_COMMAND_TYPE_STOP: {
      ESP_LOGI(kTag, "Stop command received");
      g_servo_controller.Stop();
      SendStatusResponse(cmd.id);
      break;
    }

    case app_CommandType_COMMAND_TYPE_GET_STATUS: {
      ESP_LOGI(kTag, "Get status command received");
      SendStatusResponse(cmd.id);
      break;
    }

    case app_CommandType_COMMAND_TYPE_SET_CONFIG: {
      ESP_LOGI(kTag, "Set config command received");
      if (cmd.which_payload == app_Command_set_config_tag && cmd.payload.set_config.has_config) {
        const auto& config = cmd.payload.set_config.config;
        ESP_LOGI(kTag, "Config: speed=%.2f, smoothing=%.2f, dead_zone=%.2f", static_cast<double>(config.servo_speed),
                 static_cast<double>(config.smoothing), static_cast<double>(config.dead_zone));

        // Update servo configuration
        embedded::ServoConfig servo_config;
        servo_config.speed = config.servo_speed > 0.0F ? config.servo_speed : 1.0F;
        servo_config.smoothing = config.smoothing >= 0.0F ? config.smoothing : 0.5F;
        servo_config.dead_zone = config.dead_zone >= 0.0F ? config.dead_zone : 1.0F;
        servo_config.pan_min = config.pan_min;
        servo_config.pan_max = config.pan_max;
        servo_config.tilt_min = config.tilt_min;
        servo_config.tilt_max = config.tilt_max;
        servo_config.invert_pan = config.invert_pan;
        servo_config.invert_tilt = config.invert_tilt;

        g_servo_controller.UpdateConfig(servo_config);
        SendStatusResponse(cmd.id);
      } else {
        SendErrorResponse(cmd.id, app_StatusCode_STATUS_CODE_INVALID_COMMAND, "Missing configuration");
      }
      break;
    }

    case app_CommandType_COMMAND_TYPE_PING: {
      ESP_LOGD(kTag, "Ping received, sending pong...");
      SendPingResponse(cmd.id, cmd.timestamp_ms);
      break;
    }

    default:
      ESP_LOGW(kTag, "Unknown command type: %d", cmd.type);
      SendErrorResponse(cmd.id, app_StatusCode_STATUS_CODE_INVALID_COMMAND, "Unknown command type");
      break;
  }
}

/**
 * @brief Callback for Bluetooth state changes.
 */
void OnBluetoothStateChanged(embedded::BluetoothState state) {
  switch (state) {
    case embedded::BluetoothState::kConnected:
      ESP_LOGI(kTag, "Client connected!");
      break;
    case embedded::BluetoothState::kInitialized:
      ESP_LOGI(kTag, "Bluetooth ready, waiting for connection...");
      break;
    case embedded::BluetoothState::kError:
      ESP_LOGE(kTag, "Bluetooth error!");
      break;
    default:
      break;
  }
}

/**
 * @brief Callback for received Bluetooth data.
 */
void OnBluetoothDataReceived(std::span<const uint8_t> data) {
  ESP_LOGD(kTag, "Received %zu bytes", data.size());

  // Decode command
  app_Command cmd = app_Command_init_zero;
  pb_istream_t stream = pb_istream_from_buffer(data.data(), data.size());

  if (pb_decode(&stream, app_Command_fields, &cmd)) {
    ProcessCommand(cmd);
  } else {
    ESP_LOGW(kTag, "Failed to decode command: %s", PB_GET_ERROR(&stream));
  }
}

/**
 * @brief Servo control task.
 */
void ServoTask(void* /*param*/) {
  ESP_LOGI(kTag, "Servo task started");

  uint64_t last_update_time = esp_timer_get_time() / 1000ULL;

  while (true) {
    const uint64_t current_time = esp_timer_get_time() / 1000ULL;
    const uint32_t delta_time = static_cast<uint32_t>(current_time - last_update_time);
    last_update_time = current_time;

    // Update servo controller
    g_servo_controller.Update(delta_time);

    vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz update rate
  }
}

}  // namespace

extern "C" void app_main() {
  // Initialize NVS (Non-Volatile Storage) - required for Bluetooth
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  ESP_LOGI(kTag, "========================================");
  ESP_LOGI(kTag, "  ESP32 Face Tracker Firmware");
  ESP_LOGI(kTag, "========================================");
  ESP_LOGI(kTag, "Free heap: %lu bytes", esp_get_free_heap_size());

  // Initialize servo controller
  embedded::ServoConfig servo_config;
  servo_config.pan_min = -90.0F;
  servo_config.pan_max = 90.0F;
  servo_config.tilt_min = -45.0F;
  servo_config.tilt_max = 45.0F;
  servo_config.speed = 1.0F;
  servo_config.smoothing = 0.5F;
  servo_config.dead_zone = 1.0F;
  servo_config.invert_pan = false;
  servo_config.invert_tilt = false;

  ret = g_servo_controller.Initialize(servo_config);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize servo controller: %s", esp_err_to_name(ret));
    return;
  }

  // Create command queue
  g_command_queue = xQueueCreate(kCommandQueueSize, sizeof(CommandBuffer));
  if (!g_command_queue) {
    ESP_LOGE(kTag, "Failed to create command queue");
    return;
  }

  // Initialize Bluetooth
  auto& bt = embedded::BluetoothSpp::Instance();
  bt.SetStateCallback(OnBluetoothStateChanged);
  bt.SetDataCallback(OnBluetoothDataReceived);

  ret = bt.Initialize(kDeviceName);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize Bluetooth: %s", esp_err_to_name(ret));
    return;
  }

  // Create servo control task
  xTaskCreate(ServoTask, "servo_task", 4096, nullptr, 5, nullptr);

  ESP_LOGI(kTag, "Initialization complete");
  ESP_LOGI(kTag, "Device name: %s", kDeviceName);
  ESP_LOGI(kTag, "Waiting for Bluetooth connection...");

  // Main loop - just print status periodically
  while (true) {
    const auto state = g_servo_controller.State();
    ESP_LOGI(kTag, "Status: BT=%s, Heap=%lu bytes, Servo=[%.1f, %.1f] %s", bt.Connected() ? "connected" : "waiting",
             esp_get_free_heap_size(), static_cast<double>(state.pan), static_cast<double>(state.tilt),
             state.is_moving ? "moving" : "idle");

    vTaskDelay(pdMS_TO_TICKS(10000));  // Print status every 10 seconds
  }
}
