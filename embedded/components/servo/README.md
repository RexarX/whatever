# Servo Controller Component

## Overview

This component provides PWM-based servo control for the ESP32 face tracker project. It manages two servos (pan and tilt) using the ESP32's MCPWM (Motor Control PWM) hardware peripheral.

## Features

- **Hardware PWM Control**: Uses ESP32's MCPWM peripheral for precise, jitter-free servo control
- **Dual Servo Support**: Controls pan (horizontal) and tilt (vertical) servos independently
- **Smooth Movement**: Interpolates between positions for smooth tracking
- **Configurable Limits**: Adjustable angle ranges, speed, and dead zones
- **Calibration**: Built-in calibration routine to test servo range

## Hardware Configuration

### Default GPIO Pins

- **Pan Servo**: GPIO 12
- **Tilt Servo**: GPIO 14

These can be changed via the `ServoConfig` structure.

### Servo Specifications

The controller is configured for standard hobby servos:

- **PWM Frequency**: 50Hz (20ms period)
- **Pulse Width Range**: 500µs - 2500µs
- **Center Position**: 1500µs (0°)
- **Angle Range**: -90° to +90° (configurable)

## Usage

### Initialization

```cpp
#include <servo_controller.hpp>

embedded::ServoController servo;

// Configure servo parameters
embedded::ServoConfig config;
config.pan_gpio = 16;
config.tilt_gpio = 17;
config.pan_min = -90.0F;
config.pan_max = 90.0F;
config.tilt_min = -45.0F;
config.tilt_max = 45.0F;
config.speed = 1.0F;          // 0.0 - 1.0
config.smoothing = 0.5F;      // 0.0 - 1.0
config.dead_zone = 1.0F;      // degrees
config.servo_min_pulse_us = 500;
config.servo_max_pulse_us = 2500;
config.servo_center_pulse_us = 1500;

// Initialize the controller
esp_err_t ret = servo.Initialize(config);
if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize servo controller");
}
```

### Moving Servos

```cpp
// Move to specific angles (smooth interpolation)
servo.MoveTo(45.0F, -20.0F, true);

// Move immediately (no interpolation)
servo.MoveTo(0.0F, 0.0F, false);

// Move to home position (0, 0)
servo.Home();
```

### Update Loop

The controller requires periodic updates to perform smooth interpolation:

```cpp
void servo_task(void* param) {
    uint64_t last_time = esp_timer_get_time() / 1000ULL;

    while (true) {
        uint64_t current_time = esp_timer_get_time() / 1000ULL;
        uint32_t delta_ms = (uint32_t)(current_time - last_time);
        last_time = current_time;

        servo.Update(delta_ms);

        vTaskDelay(pdMS_TO_TICKS(20)); // 50Hz update rate
    }
}
```

### Configuration Update

```cpp
// Update configuration at runtime
embedded::ServoConfig new_config = config;
new_config.speed = 0.5F;  // Slower movement
new_config.smoothing = 0.7F;  // More smoothing

servo.UpdateConfig(new_config);
```

### Calibration

```cpp
// Run calibration sequence
servo.Calibrate();
// The servos will move through their full range to verify operation
```

## API Reference

### ServoConfig

| Field                   | Type     | Default | Description                  |
| ----------------------- | -------- | ------- | ---------------------------- |
| `pan_gpio`              | int      | 16      | GPIO pin for pan servo       |
| `tilt_gpio`             | int      | 17      | GPIO pin for tilt servo      |
| `pan_min`               | float    | -90.0   | Minimum pan angle (degrees)  |
| `pan_max`               | float    | 90.0    | Maximum pan angle (degrees)  |
| `tilt_min`              | float    | -45.0   | Minimum tilt angle (degrees) |
| `tilt_max`              | float    | 45.0    | Maximum tilt angle (degrees) |
| `speed`                 | float    | 1.0     | Movement speed (0.0-1.0)     |
| `smoothing`             | float    | 0.5     | Smoothing factor (0.0-1.0)   |
| `dead_zone`             | float    | 1.0     | Dead zone in degrees         |
| `invert_pan`            | bool     | false   | Invert pan direction         |
| `invert_tilt`           | bool     | false   | Invert tilt direction        |
| `servo_min_pulse_us`    | uint32_t | 500     | Minimum pulse width (µs)     |
| `servo_max_pulse_us`    | uint32_t | 2500    | Maximum pulse width (µs)     |
| `servo_center_pulse_us` | uint32_t | 1500    | Center pulse width (µs)      |

### ServoState

| Field           | Type  | Description                         |
| --------------- | ----- | ----------------------------------- |
| `pan`           | float | Current pan position (degrees)      |
| `tilt`          | float | Current tilt position (degrees)     |
| `target_pan`    | float | Target pan position (degrees)       |
| `target_tilt`   | float | Target tilt position (degrees)      |
| `is_moving`     | bool  | Whether servos are currently moving |
| `is_calibrated` | bool  | Whether servos are calibrated       |

### Methods

#### `esp_err_t Initialize(const ServoConfig& config)`

Initializes the servo controller with the given configuration. Must be called before using other methods.

**Returns**: `ESP_OK` on success, error code otherwise.

#### `void Update(uint32_t delta_time_ms)`

Updates servo positions for smooth interpolation. Should be called periodically (e.g., every 20ms).

**Parameters**:

- `delta_time_ms`: Time elapsed since last update in milliseconds

#### `void MoveTo(float pan, float tilt, bool smooth = true)`

Moves servos to the specified position.

**Parameters**:

- `pan`: Target pan angle in degrees
- `tilt`: Target tilt angle in degrees
- `smooth`: Use smooth interpolation if true, immediate movement if false

#### `void Home()`

Moves servos to home position (0, 0).

#### `void Stop()`

Stops servo movement immediately by setting target to current position.

#### `void Calibrate()`

Runs a calibration sequence that moves servos through their full range.

#### `ServoState State() const`

Returns the current servo state.

#### `void UpdateConfig(const ServoConfig& config)`

Updates the servo configuration at runtime.

#### `bool IsMoving() const`

Returns true if servos are currently moving.

#### `bool IsCalibrated() const`

Returns true if servos are calibrated.

## Implementation Details

### PWM Generation

The controller uses ESP32's MCPWM peripheral with the following configuration:

- **Timer Resolution**: 1MHz (1µs tick)
- **Timer Period**: 20,000 ticks (20ms)
- **Comparator Mode**: Updates on timer zero
- **Generator Actions**:
  - Timer empty event → Set output HIGH
  - Comparator match → Set output LOW

This generates a standard servo PWM signal with configurable pulse width.

### Angle to Pulse Width Conversion

The `AngleToPulseWidth()` function converts angles to pulse widths:

```
angle: -90° to +90°
normalized = angle / 90.0  (-1.0 to +1.0)

if normalized < 0:
    pulse = center + normalized * (center - min)
else:
    pulse = center + normalized * (max - center)
```

### Smooth Movement

Smooth movement uses exponential interpolation:

```
new_position = current + (target - current) * smooth_factor
smooth_factor = smoothing * speed * time_factor
```

Movement stops when the position is within the dead zone threshold.

## Troubleshooting

### Servo Jitter or Glitches

- Ensure adequate power supply (servos can draw significant current)
- Check for ground loops or noisy power
- Reduce `speed` or increase `smoothing` values

### Servo Not Moving

- Verify GPIO pins are correct and not used by other peripherals
- Check servo power supply (5V, adequate current)
- Verify servo signal wire is connected to correct GPIO
- Check logs for initialization errors

### Incorrect Range

- Adjust `servo_min_pulse_us`, `servo_max_pulse_us`, and `servo_center_pulse_us`
- Some servos use different pulse width ranges (e.g., 600-2400µs)
- Use `invert_pan` or `invert_tilt` to reverse direction

## Dependencies

- `esp_timer`: For timing functions
- `driver`: For MCPWM peripheral driver
- `freertos`: For task delays in calibration

## License

See project LICENSE file.
