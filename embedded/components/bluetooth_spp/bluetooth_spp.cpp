#include "bluetooth_spp.hpp"

#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_log.h>
#include <esp_spp_api.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>

namespace embedded {

namespace {

constexpr const char* kTag = "BluetoothSpp";

// SPP server name
constexpr const char* kSppServerName = "SPP_SERVER";

// Global instance pointer for callbacks
BluetoothSpp* g_instance = nullptr;

}  // namespace

// C-style callback wrappers
void SppCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t* param) {
  if (g_instance) {
    g_instance->HandleSppEvent(static_cast<uint32_t>(event), param);
  }
}

void GapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (g_instance) {
    g_instance->HandleGapEvent(static_cast<uint32_t>(event), param);
  }
}

BluetoothSpp::BluetoothSpp() {
  g_instance = this;
}

BluetoothSpp::~BluetoothSpp() {
  if (Initialized()) {
    Deinitialize();
  }
  if (g_instance == this) {
    g_instance = nullptr;
  }
}

esp_err_t BluetoothSpp::Initialize(std::string_view device_name) {
  if (Initialized()) {
    ESP_LOGW(kTag, "Already initialized");
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Initializing Bluetooth SPP...");

  // Release memory for BLE (we only use classic Bluetooth)
  esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
  if (ret != ESP_OK) {
    ESP_LOGW(kTag, "Failed to release BLE memory: %s", esp_err_to_name(ret));
    // Not fatal, continue
  }

  // Initialize Bluetooth controller with default config
  esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  ret = esp_bt_controller_init(&bt_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize Bluetooth controller: %s", esp_err_to_name(ret));
    SetState(BluetoothState::kError);
    return ret;
  }

  // Enable Bluetooth controller in classic mode
  ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to enable Bluetooth controller: %s", esp_err_to_name(ret));
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Initialize Bluedroid stack
  ret = esp_bluedroid_init();
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize Bluedroid: %s", esp_err_to_name(ret));
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Enable Bluedroid stack
  ret = esp_bluedroid_enable();
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to enable Bluedroid: %s", esp_err_to_name(ret));
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Register GAP callback
  ret = esp_bt_gap_register_callback(GapCallback);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to register GAP callback: %s", esp_err_to_name(ret));
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Register SPP callback
  ret = esp_spp_register_callback(SppCallback);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to register SPP callback: %s", esp_err_to_name(ret));
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Initialize SPP
  esp_spp_cfg_t spp_cfg = {
      .mode = ESP_SPP_MODE_CB,
      .enable_l2cap_ertm = false,
      .tx_buffer_size = 0,
  };
  ret = esp_spp_enhanced_init(&spp_cfg);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to initialize SPP: %s", esp_err_to_name(ret));
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    SetState(BluetoothState::kError);
    return ret;
  }

  // Set device name
  std::string name_str(device_name);
  ret = esp_bt_dev_set_device_name(name_str.c_str());
  if (ret != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set device name: %s", esp_err_to_name(ret));
    // Not fatal
  }

  // Set discoverable and connectable mode
  ret = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  if (ret != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set scan mode: %s", esp_err_to_name(ret));
    // Not fatal
  }

  SetState(BluetoothState::kInitialized);
  ESP_LOGI(kTag, "Bluetooth SPP initialized successfully");
  ESP_LOGI(kTag, "Device name: %.*s", static_cast<int>(device_name.size()), device_name.data());

  return ESP_OK;
}

esp_err_t BluetoothSpp::Deinitialize() {
  if (!Initialized()) {
    return ESP_OK;
  }

  ESP_LOGI(kTag, "Deinitializing Bluetooth SPP...");

  esp_spp_deinit();
  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  connection_handle_ = 0;
  SetState(BluetoothState::kUninitialized);

  ESP_LOGI(kTag, "Bluetooth SPP deinitialized");
  return ESP_OK;
}

int BluetoothSpp::Send(std::span<const uint8_t> data) {
  if (!Connected()) {
    ESP_LOGW(kTag, "Cannot send: not connected");
    return -1;
  }

  if (data.empty()) {
    return 0;
  }

  if (data.size() > kMaxPacketSize) {
    ESP_LOGW(kTag, "Data size %zu exceeds maximum packet size %zu", data.size(), kMaxPacketSize);
    return -1;
  }

  esp_err_t ret = esp_spp_write(connection_handle_, static_cast<int>(data.size()), const_cast<uint8_t*>(data.data()));
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to send data: %s", esp_err_to_name(ret));
    return -1;
  }

  return static_cast<int>(data.size());
}

void BluetoothSpp::HandleSppEvent(uint32_t event, void* param) {
  auto* spp_param = static_cast<esp_spp_cb_param_t*>(param);

  switch (static_cast<esp_spp_cb_event_t>(event)) {
    case ESP_SPP_INIT_EVT:
      if (spp_param->init.status == ESP_SPP_SUCCESS) {
        ESP_LOGI(kTag, "SPP initialized, starting server...");
        // Start SPP server
        esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, kSppServerName);
      } else {
        ESP_LOGE(kTag, "SPP init failed: %d", spp_param->init.status);
        SetState(BluetoothState::kError);
      }
      break;

    case ESP_SPP_START_EVT:
      if (spp_param->start.status == ESP_SPP_SUCCESS) {
        ESP_LOGI(kTag, "SPP server started, waiting for connections...");
      } else {
        ESP_LOGE(kTag, "SPP server start failed: %d", spp_param->start.status);
        SetState(BluetoothState::kError);
      }
      break;

    case ESP_SPP_SRV_OPEN_EVT:
      if (spp_param->srv_open.status == ESP_SPP_SUCCESS) {
        connection_handle_ = spp_param->srv_open.handle;
        ESP_LOGI(kTag, "Client connected, handle: %lu", connection_handle_);
        SetState(BluetoothState::kConnected);
      } else {
        ESP_LOGE(kTag, "Client connection failed: %d", spp_param->srv_open.status);
      }
      break;

    case ESP_SPP_CLOSE_EVT:
      ESP_LOGI(kTag, "Connection closed");
      connection_handle_ = 0;
      SetState(BluetoothState::kInitialized);
      break;

    case ESP_SPP_DATA_IND_EVT:
      if (spp_param->data_ind.len > 0 && data_callback_) {
        std::span<const uint8_t> data(spp_param->data_ind.data, static_cast<size_t>(spp_param->data_ind.len));
        data_callback_(data);
      }
      break;

    case ESP_SPP_WRITE_EVT:
      if (spp_param->write.status != ESP_SPP_SUCCESS) {
        ESP_LOGW(kTag, "Write failed: %d", spp_param->write.status);
      }
      break;

    case ESP_SPP_CONG_EVT:
      ESP_LOGD(kTag, "Congestion event: %s", spp_param->cong.cong ? "congested" : "clear");
      break;

    default:
      ESP_LOGD(kTag, "Unhandled SPP event: %lu", event);
      break;
  }
}

void BluetoothSpp::HandleGapEvent(uint32_t event, void* param) {
  auto* gap_param = static_cast<esp_bt_gap_cb_param_t*>(param);

  switch (static_cast<esp_bt_gap_cb_event_t>(event)) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:
      if (gap_param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
        ESP_LOGI(kTag, "Authentication complete: %s", gap_param->auth_cmpl.device_name);
      } else {
        ESP_LOGW(kTag, "Authentication failed: %d", gap_param->auth_cmpl.stat);
      }
      break;

    case ESP_BT_GAP_PIN_REQ_EVT:
      ESP_LOGI(kTag, "PIN request - using default PIN 1234");
      {
        esp_bt_pin_code_t pin = {'1', '2', '3', '4'};
        esp_bt_gap_pin_reply(gap_param->pin_req.bda, true, 4, pin);
      }
      break;

    case ESP_BT_GAP_CFM_REQ_EVT:
      ESP_LOGI(kTag, "Confirm request, accepting...");
      esp_bt_gap_ssp_confirm_reply(gap_param->cfm_req.bda, true);
      break;

    case ESP_BT_GAP_KEY_NOTIF_EVT:
      ESP_LOGI(kTag, "Passkey notification: %lu", gap_param->key_notif.passkey);
      break;

    case ESP_BT_GAP_KEY_REQ_EVT:
      ESP_LOGI(kTag, "Passkey request");
      break;

    case ESP_BT_GAP_MODE_CHG_EVT:
      ESP_LOGD(kTag, "Power mode changed: %d", gap_param->mode_chg.mode);
      break;

    default:
      ESP_LOGD(kTag, "Unhandled GAP event: %lu", event);
      break;
  }
}

void BluetoothSpp::SetState(BluetoothState state) {
  if (state_ != state) {
    ESP_LOGI(kTag, "State changed: %d -> %d", static_cast<int>(state_), static_cast<int>(state));
    state_ = state;

    if (state_callback_) {
      state_callback_(state);
    }
  }
}

}  // namespace embedded
