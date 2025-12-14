/**
 * @file servo_controller.cpp
 * @brief Servo controller implementation
 */

#include "include/servo_controller.hpp"

#include <esp_log.h>
#include <esp_timer.h>

#include <algorithm>
#include <cmath>

namespace embedded {

namespace {
constexpr const char* kTag = "servo";
constexpr float kMinMovement = 0.1F;  // Minimum movement threshold in degrees
}  // namespace

esp_err_t ServoController::Initialize(const ServoConfig& config) noexcept {
  if (initialized_) {
    ESP_LOGW(kTag, "Servo controller already initialized");
    return ESP_OK;
  }

  config_ = config;
  state_.pan = 0.0F;
  state_.tilt = 0.0F;
  state_.target_pan = 0.0F;
  state_.target_tilt = 0.0F;
  state_.is_moving = false;
  state_.is_calibrated = true;
  initialized_ = true;
  last_move_time_ = esp_timer_get_time() / 1000ULL;

  ESP_LOGI(kTag, "Servo controller initialized");
  ESP_LOGI(kTag, "  Pan range: [%.1f, %.1f] deg", static_cast<double>(config_.pan_min),
           static_cast<double>(config_.pan_max));
  ESP_LOGI(kTag, "  Tilt range: [%.1f, %.1f] deg", static_cast<double>(config_.tilt_min),
           static_cast<double>(config_.tilt_max));
  ESP_LOGI(kTag, "  Speed: %.2f, Smoothing: %.2f, Dead zone: %.2f deg", static_cast<double>(config_.speed),
           static_cast<double>(config_.smoothing), static_cast<double>(config_.dead_zone));

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
    ESP_LOGI(kTag, "Servos reached target position: pan=%.2f deg, tilt=%.2f deg", static_cast<double>(state_.pan),
             static_cast<double>(state_.tilt));
  } else {
    // Still moving
    const bool position_changed =
        std::abs(new_pan - state_.pan) > kMinMovement || std::abs(new_tilt - state_.tilt) > kMinMovement;

    state_.pan = new_pan;
    state_.tilt = new_tilt;

    if (position_changed) {
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

  // Simulate calibration by moving through key positions
  ESP_LOGI(kTag, "[Calibration] Step 1: Moving to center position");
  LogServoMove(0.0F, 0.0F);

  ESP_LOGI(kTag, "[Calibration] Step 2: Testing pan range");
  LogServoMove(config_.pan_max, 0.0F);
  LogServoMove(config_.pan_min, 0.0F);

  ESP_LOGI(kTag, "[Calibration] Step 3: Testing tilt range");
  LogServoMove(0.0F, config_.tilt_max);
  LogServoMove(0.0F, config_.tilt_min);

  ESP_LOGI(kTag, "[Calibration] Step 4: Returning to center");
  LogServoMove(0.0F, 0.0F);

  // Update state
  state_.pan = 0.0F;
  state_.tilt = 0.0F;
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

}  // namespace embedded
