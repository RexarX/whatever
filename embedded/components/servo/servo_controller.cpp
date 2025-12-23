/**
 * @file servo_controller.cpp
 * @brief Servo controller implementation
 */

#include "include/servo_controller.hpp"

#include <driver/mcpwm_prelude.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <algorithm>
#include <cmath>

namespace embedded {

namespace {
constexpr const char* kTag = "servo";
constexpr float kMinMovement = 0.1F;           // Minimum movement threshold in degrees
constexpr uint32_t kServoPwmFrequency = 50;    // 50Hz for standard servos
constexpr uint32_t kServoPwmPeriodUs = 20000;  // 20ms period (1/50Hz)
}  // namespace

esp_err_t ServoController::Initialize(const ServoConfig& config) noexcept {
  if (initialized_) {
    ESP_LOGW(kTag, "Servo controller already initialized");
    return ESP_OK;
  }

  config_ = config;

  // Create MCPWM timer for pan servo
  ESP_LOGI(kTag, "Initializing pan servo on GPIO %d", config_.pan_gpio);
  mcpwm_timer_config_t timer_config = {};
  timer_config.group_id = 0;
  timer_config.clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT;
  timer_config.resolution_hz = 1000000;  // 1MHz, 1 tick = 1us
  timer_config.count_mode = MCPWM_TIMER_COUNT_MODE_UP;
  timer_config.period_ticks = kServoPwmPeriodUs;  // 20ms period

  esp_err_t ret = mcpwm_new_timer(&timer_config, &pan_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create pan timer: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create MCPWM operator for pan servo
  mcpwm_operator_config_t operator_config = {};
  operator_config.group_id = 0;

  ret = mcpwm_new_operator(&operator_config, &pan_operator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create pan operator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Connect timer and operator
  ret = mcpwm_operator_connect_timer(pan_operator_, pan_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to connect pan timer and operator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create comparator for pan servo
  mcpwm_comparator_config_t comparator_config = {};
  comparator_config.flags.update_cmp_on_tez = true;

  ret = mcpwm_new_comparator(pan_operator_, &comparator_config, &pan_comparator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create pan comparator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create generator for pan servo
  mcpwm_generator_config_t generator_config = {};
  generator_config.gen_gpio_num = config_.pan_gpio;

  ret = mcpwm_new_generator(pan_operator_, &generator_config, &pan_generator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create pan generator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Set generator actions for pan servo
  ret = mcpwm_generator_set_action_on_timer_event(
      pan_generator_,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set pan generator timer action: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_generator_set_action_on_compare_event(
      pan_generator_, MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, pan_comparator_, MCPWM_GEN_ACTION_LOW));
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set pan generator compare action: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create MCPWM timer for tilt servo
  ESP_LOGI(kTag, "Initializing tilt servo on GPIO %d", config_.tilt_gpio);
  timer_config.group_id = 0;

  ret = mcpwm_new_timer(&timer_config, &tilt_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create tilt timer: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create MCPWM operator for tilt servo
  operator_config.group_id = 0;

  ret = mcpwm_new_operator(&operator_config, &tilt_operator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create tilt operator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Connect timer and operator
  ret = mcpwm_operator_connect_timer(tilt_operator_, tilt_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to connect tilt timer and operator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create comparator for tilt servo
  ret = mcpwm_new_comparator(tilt_operator_, &comparator_config, &tilt_comparator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create tilt comparator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Create generator for tilt servo
  generator_config.gen_gpio_num = config_.tilt_gpio;

  ret = mcpwm_new_generator(tilt_operator_, &generator_config, &tilt_generator_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to create tilt generator: %s", esp_err_to_name(ret));
    return ret;
  }

  // Set generator actions for tilt servo
  ret = mcpwm_generator_set_action_on_timer_event(
      tilt_generator_,
      MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH));
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set tilt generator timer action: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_generator_set_action_on_compare_event(
      tilt_generator_,
      MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, tilt_comparator_, MCPWM_GEN_ACTION_LOW));
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to set tilt generator compare action: %s", esp_err_to_name(ret));
    return ret;
  }

  // Enable and start timers
  ret = mcpwm_timer_enable(pan_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to enable pan timer: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_timer_start_stop(pan_timer_, MCPWM_TIMER_START_NO_STOP);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to start pan timer: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_timer_enable(tilt_timer_);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to enable tilt timer: %s", esp_err_to_name(ret));
    return ret;
  }

  ret = mcpwm_timer_start_stop(tilt_timer_, MCPWM_TIMER_START_NO_STOP);
  if (ret != ESP_OK) {
    ESP_LOGE(kTag, "Failed to start tilt timer: %s", esp_err_to_name(ret));
    return ret;
  }

  // Initialize state
  state_.pan = 0.0F;
  state_.tilt = 0.0F;
  state_.target_pan = 0.0F;
  state_.target_tilt = 0.0F;
  state_.is_moving = false;
  state_.is_calibrated = true;
  initialized_ = true;
  last_move_time_ = esp_timer_get_time() / 1000ULL;

  // Move servos to home position (center)
  ApplyServoPositions();

  ESP_LOGI(kTag, "Servo controller initialized");
  ESP_LOGI(kTag, "  Pan GPIO: %d, Tilt GPIO: %d", config_.pan_gpio, config_.tilt_gpio);
  ESP_LOGI(kTag, "  Pan range: [%.1f, %.1f] deg", static_cast<double>(config_.pan_min),
           static_cast<double>(config_.pan_max));
  ESP_LOGI(kTag, "  Tilt range: [%.1f, %.1f] deg", static_cast<double>(config_.tilt_min),
           static_cast<double>(config_.tilt_max));
  ESP_LOGI(kTag, "  Speed: %.2f, Smoothing: %.2f, Dead zone: %.2f deg", static_cast<double>(config_.speed),
           static_cast<double>(config_.smoothing), static_cast<double>(config_.dead_zone));
  ESP_LOGI(kTag, "  Pulse range: [%lu, %lu] us, Center: %lu us", static_cast<unsigned long>(config_.servo_min_pulse_us),
           static_cast<unsigned long>(config_.servo_max_pulse_us),
           static_cast<unsigned long>(config_.servo_center_pulse_us));

  return ESP_OK;
}

void ServoController::Update(uint32_t delta_time_ms) noexcept {
  if (!initialized_ || !state_.is_moving) {
    return;
  }

  // Calculate smoothing factor based on speed and delta time
  const float time_factor = static_cast<float>(delta_time_ms) / 20.0F;  // Normalized to 20ms updates
  const float smooth_factor = config_.smoothing * config_.speed * time_factor;

  // Apply smoothing to pan
  const float new_pan = SmoothMove(state_.pan, state_.target_pan, smooth_factor);
  const float new_tilt = SmoothMove(state_.tilt, state_.target_tilt, smooth_factor);

  // Check if we've reached the target (within dead zone)
  const float pan_diff = std::abs(new_pan - state_.target_pan);
  const float tilt_diff = std::abs(new_tilt - state_.target_tilt);

  if (pan_diff < kMinMovement && tilt_diff < kMinMovement) {
    // Reached target
    state_.pan = state_.target_pan;
    state_.tilt = state_.target_tilt;
    state_.is_moving = false;
    ApplyServoPositions();
    ESP_LOGI(kTag, "Servos reached target position: pan=%.2f deg, tilt=%.2f deg", static_cast<double>(state_.pan),
             static_cast<double>(state_.tilt));
  } else {
    // Still moving
    const bool position_changed =
        std::abs(new_pan - state_.pan) > kMinMovement || std::abs(new_tilt - state_.tilt) > kMinMovement;

    state_.pan = new_pan;
    state_.tilt = new_tilt;

    if (position_changed) {
      ApplyServoPositions();
      LogServoMove(state_.pan, state_.tilt);
    }
  }
}

void ServoController::MoveTo(float pan, float tilt, bool smooth) noexcept {
  if (!initialized_) {
    ESP_LOGW(kTag, "Cannot move servos: not initialized");
    return;
  }

  // Apply inversion
  if (config_.invert_pan) {
    pan = -pan;
  }
  if (config_.invert_tilt) {
    tilt = -tilt;
  }

  // Clamp to limits
  pan = ClampAngle(pan, config_.pan_min, config_.pan_max);
  tilt = ClampAngle(tilt, config_.tilt_min, config_.tilt_max);

  // Check dead zone
  const float pan_diff = std::abs(pan - state_.pan);
  const float tilt_diff = std::abs(tilt - state_.tilt);

  if (pan_diff < config_.dead_zone && tilt_diff < config_.dead_zone) {
    ESP_LOGD(kTag, "Movement within dead zone, ignoring");
    return;
  }

  state_.target_pan = pan;
  state_.target_tilt = tilt;

  if (!smooth) {
    // Immediate movement
    state_.pan = pan;
    state_.tilt = tilt;
    state_.is_moving = false;
    ApplyServoPositions();
    LogServoMove(state_.pan, state_.tilt);
    ESP_LOGI(kTag, "Servos moved immediately to: pan=%.2f deg, tilt=%.2f deg", static_cast<double>(state_.pan),
             static_cast<double>(state_.tilt));
  } else {
    // Smooth movement
    state_.is_moving = true;
    ESP_LOGI(kTag, "Servos moving to target: pan=%.2f deg, tilt=%.2f deg", static_cast<double>(pan),
             static_cast<double>(tilt));
  }

  last_move_time_ = esp_timer_get_time() / 1000ULL;
}

void ServoController::Home() noexcept {
  ESP_LOGI(kTag, "Moving servos to home position");
  MoveTo(0.0F, 0.0F, true);
}

void ServoController::Stop() noexcept {
  if (!initialized_) {
    return;
  }

  ESP_LOGI(kTag, "Stopping servo movement");
  state_.target_pan = state_.pan;
  state_.target_tilt = state_.tilt;
  state_.is_moving = false;
}

void ServoController::Calibrate() noexcept {
  if (!initialized_) {
    ESP_LOGW(kTag, "Cannot calibrate: not initialized");
    return;
  }

  ESP_LOGI(kTag, "Starting calibration sequence...");

  // Move through key positions to test servo range
  ESP_LOGI(kTag, "[Calibration] Step 1: Moving to center position");
  state_.pan = 0.0F;
  state_.tilt = 0.0F;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_LOGI(kTag, "[Calibration] Step 2: Testing pan range");
  state_.pan = config_.pan_max;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);
  vTaskDelay(pdMS_TO_TICKS(500));

  state_.pan = config_.pan_min;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_LOGI(kTag, "[Calibration] Step 3: Testing tilt range");
  state_.pan = 0.0F;
  state_.tilt = config_.tilt_max;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);
  vTaskDelay(pdMS_TO_TICKS(500));

  state_.tilt = config_.tilt_min;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);
  vTaskDelay(pdMS_TO_TICKS(500));

  ESP_LOGI(kTag, "[Calibration] Step 4: Returning to center");
  state_.pan = 0.0F;
  state_.tilt = 0.0F;
  ApplyServoPositions();
  LogServoMove(state_.pan, state_.tilt);

  // Update state
  state_.target_pan = 0.0F;
  state_.target_tilt = 0.0F;
  state_.is_moving = false;
  state_.is_calibrated = true;

  ESP_LOGI(kTag, "Calibration complete!");
}

ServoState ServoController::State() const noexcept {
  return state_;
}

void ServoController::UpdateConfig(const ServoConfig& config) noexcept {
  config_ = config;
  ESP_LOGI(kTag, "Servo configuration updated");
  ESP_LOGI(kTag, "  Speed: %.2f, Smoothing: %.2f, Dead zone: %.2f deg", static_cast<double>(config_.speed),
           static_cast<double>(config_.smoothing), static_cast<double>(config_.dead_zone));
}

void ServoController::LogServoMove(float pan, float tilt) const noexcept {
  ESP_LOGI(kTag, ">>> SERVO MOVE: pan=%.2f deg, tilt=%.2f deg <<<", static_cast<double>(pan),
           static_cast<double>(tilt));
}

esp_err_t ServoController::SetServoPulse(mcpwm_cmpr_handle_t comparator, uint32_t pulse_width_us) noexcept {
  return mcpwm_comparator_set_compare_value(comparator, pulse_width_us);
}

void ServoController::ApplyServoPositions() noexcept {
  if (!initialized_) {
    return;
  }

  // Convert angles to pulse widths
  const uint32_t pan_pulse = AngleToPulseWidth(state_.pan, config_.servo_min_pulse_us, config_.servo_max_pulse_us,
                                               config_.servo_center_pulse_us);
  const uint32_t tilt_pulse = AngleToPulseWidth(state_.tilt, config_.servo_min_pulse_us, config_.servo_max_pulse_us,
                                                config_.servo_center_pulse_us);

  // Set PWM pulse widths
  esp_err_t ret = SetServoPulse(pan_comparator_, pan_pulse);
  if (ret != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set pan servo pulse: %s", esp_err_to_name(ret));
  }

  ret = SetServoPulse(tilt_comparator_, tilt_pulse);
  if (ret != ESP_OK) {
    ESP_LOGW(kTag, "Failed to set tilt servo pulse: %s", esp_err_to_name(ret));
  }

  ESP_LOGD(kTag, "Applied servo positions: pan=%.2f deg (%lu us), tilt=%.2f deg (%lu us)",
           static_cast<double>(state_.pan), static_cast<unsigned long>(pan_pulse), static_cast<double>(state_.tilt),
           static_cast<unsigned long>(tilt_pulse));
}

}  // namespace embedded
