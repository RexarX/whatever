/**
 * @file servo_controller.hpp
 * @brief Servo controller for pan/tilt mechanism
 */

#pragma once

#include <driver/mcpwm_prelude.h>
#include <esp_err.h>

#include <atomic>
#include <cstdint>

namespace embedded {

/**
 * @brief Servo controller state.
 */
struct ServoState {
  float pan = 0.0F;           ///< Current pan position in degrees (-90 to 90).
  float tilt = 0.0F;          ///< Current tilt position in degrees (-45 to 45).
  float target_pan = 0.0F;    ///< Target pan position in degrees.
  float target_tilt = 0.0F;   ///< Target tilt position in degrees.
  bool is_moving = false;     ///< Whether servos are currently moving.
  bool is_calibrated = true;  ///< Whether servos are calibrated.
};

/**
 * @brief Servo controller configuration.
 */
struct ServoConfig {
  int pan_gpio = 12;                      ///< GPIO pin for pan servo.
  int tilt_gpio = 14;                     ///< GPIO pin for tilt servo.
  float pan_min = -90.0F;                 ///< Minimum pan angle in degrees.
  float pan_max = 90.0F;                  ///< Maximum pan angle in degrees.
  float tilt_min = -45.0F;                ///< Minimum tilt angle in degrees.
  float tilt_max = 45.0F;                 ///< Maximum tilt angle in degrees.
  float speed = 1.0F;                     ///< Movement speed (0.0 to 1.0).
  float smoothing = 0.5F;                 ///< Smoothing factor (0.0 to 1.0).
  float dead_zone = 1.0F;                 ///< Dead zone in degrees (minimum movement threshold).
  bool invert_pan = false;                ///< Invert pan direction.
  bool invert_tilt = false;               ///< Invert tilt direction.
  uint32_t servo_min_pulse_us = 500;      ///< Minimum pulse width in microseconds.
  uint32_t servo_max_pulse_us = 2500;     ///< Maximum pulse width in microseconds.
  uint32_t servo_center_pulse_us = 1500;  ///< Center pulse width in microseconds.
};

/**
 * @brief Servo controller for pan/tilt mechanism.
 * @details This class manages servo movement, calibration, and position tracking
 * using ESP32's MCPWM hardware for precise PWM control.
 */
class ServoController final {
public:
  ServoController() = default;
  ServoController(const ServoController&) = delete;
  ServoController(ServoController&&) = delete;
  ~ServoController() = default;

  ServoController& operator=(const ServoController&) = delete;
  ServoController& operator=(ServoController&&) = delete;

  /**
   * @brief Initializes the servo controller.
   * @param config Servo configuration.
   * @return ESP_OK on success, error code otherwise.
   */
  esp_err_t Initialize(const ServoConfig& config) noexcept;

  /**
   * @brief Updates servo positions (should be called periodically).
   * @details This performs smooth interpolation between current and target positions.
   * @param delta_time_ms Time elapsed since last update in milliseconds.
   */
  void Update(uint32_t delta_time_ms) noexcept;

  /**
   * @brief Moves servos to a target position.
   * @param pan Target pan angle in degrees.
   * @param tilt Target tilt angle in degrees.
   * @param smooth Whether to use smooth interpolation.
   */
  void MoveTo(float pan, float tilt, bool smooth = true) noexcept;

  /**
   * @brief Moves servos to home position (0, 0).
   */
  void Home() noexcept;

  /**
   * @brief Stops servo movement immediately.
   */
  void Stop() noexcept;

  /**
   * @brief Starts calibration process.
   * @details This would normally involve moving servos through their full range
   * to determine limits and center position. For now, it just logs the process.
   */
  void Calibrate() noexcept;

  /**
   * @brief Gets the current servo state.
   * @return Current state.
   */
  [[nodiscard]] ServoState State() const noexcept;

  /**
   * @brief Updates the servo configuration.
   * @param config New configuration.
   */
  void UpdateConfig(const ServoConfig& config) noexcept;

  /**
   * @brief Checks if servos are currently moving.
   * @return True if moving.
   */
  [[nodiscard]] bool IsMoving() const noexcept { return state_.is_moving; }

  /**
   * @brief Checks if servos are calibrated.
   * @return True if calibrated.
   */
  [[nodiscard]] bool IsCalibrated() const noexcept { return state_.is_calibrated; }

private:
  /**
   * @brief Clamps an angle to the specified range.
   * @param angle Angle to clamp.
   * @param min Minimum value.
   * @param max Maximum value.
   * @return Clamped angle.
   */
  [[nodiscard]] static constexpr float ClampAngle(float angle, float min, float max) noexcept {
    return angle < min ? min : (angle > max ? max : angle);
  }

  /**
   * @brief Applies smoothing to servo movement.
   * @param current Current position.
   * @param target Target position.
   * @param factor Smoothing factor (0.0 to 1.0).
   * @return Smoothed position.
   */
  [[nodiscard]] static constexpr float SmoothMove(float current, float target, float factor) noexcept {
    return current + (target - current) * factor;
  }

  /**
   * @brief Logs servo movement.
   * @param pan Pan position.
   * @param tilt Tilt position.
   */
  void LogServoMove(float pan, float tilt) const noexcept;

  /**
   * @brief Converts angle to pulse width in microseconds.
   * @param angle Angle in degrees.
   * @param min_pulse Minimum pulse width in microseconds.
   * @param max_pulse Maximum pulse width in microseconds.
   * @param center_pulse Center pulse width in microseconds.
   * @return Pulse width in microseconds.
   */
  [[nodiscard]] static constexpr uint32_t AngleToPulseWidth(float angle, uint32_t min_pulse, uint32_t max_pulse,
                                                            uint32_t center_pulse) noexcept {
    // angle: -90 to +90 degrees
    // -90 -> min_pulse, 0 -> center_pulse, +90 -> max_pulse
    const float normalized = angle / 90.0F;  // -1.0 to +1.0
    if (normalized < 0.0F) {
      // Interpolate between min and center
      return static_cast<uint32_t>(static_cast<float>(center_pulse) +
                                   normalized * static_cast<float>(center_pulse - min_pulse));
    } else {
      // Interpolate between center and max
      return static_cast<uint32_t>(static_cast<float>(center_pulse) +
                                   normalized * static_cast<float>(max_pulse - center_pulse));
    }
  }

  /**
   * @brief Sets the PWM duty cycle for a servo.
   * @param comparator MCPWM comparator handle.
   * @param pulse_width_us Pulse width in microseconds.
   * @return ESP_OK on success, error code otherwise.
   */
  esp_err_t SetServoPulse(mcpwm_cmpr_handle_t comparator, uint32_t pulse_width_us) noexcept;

  /**
   * @brief Actually moves the physical servos to the current state position.
   */
  void ApplyServoPositions() noexcept;

  ServoConfig config_;
  ServoState state_;
  bool initialized_ = false;
  uint64_t last_move_time_ = 0;
  mcpwm_timer_handle_t pan_timer_ = nullptr;
  mcpwm_timer_handle_t tilt_timer_ = nullptr;
  mcpwm_oper_handle_t pan_operator_ = nullptr;
  mcpwm_oper_handle_t tilt_operator_ = nullptr;
  mcpwm_cmpr_handle_t pan_comparator_ = nullptr;
  mcpwm_cmpr_handle_t tilt_comparator_ = nullptr;
  mcpwm_gen_handle_t pan_generator_ = nullptr;
  mcpwm_gen_handle_t tilt_generator_ = nullptr;
};

}  // namespace embedded
