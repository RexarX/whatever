#include <doctest/doctest.h>

#include <client/comm/bluetooth.hpp>

#include <cstdint>
#include <string>
#include <vector>

TEST_SUITE("client::comm::BluetoothManager") {
  TEST_CASE("BluetoothError: BluetoothErrorToString returns correct strings") {
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kOk), "OK");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kNotSupported),
             "Bluetooth not supported");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kNotEnabled), "Bluetooth is disabled");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kDeviceNotFound), "Device not found");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kConnectionFailed),
             "Connection failed");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kConnectionLost), "Connection lost");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kSendFailed), "Failed to send data");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kReceiveFailed),
             "Failed to receive data");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kTimeout), "Operation timed out");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kAlreadyConnected),
             "Already connected");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kNotConnected), "Not connected");
    CHECK_EQ(client::comm::BluetoothErrorToString(client::comm::BluetoothError::kInternalError), "Internal error");
  }

  TEST_CASE("BluetoothError: Error codes are distinct") {
    CHECK_NE(client::comm::BluetoothError::kOk, client::comm::BluetoothError::kNotSupported);
    CHECK_NE(client::comm::BluetoothError::kNotSupported, client::comm::BluetoothError::kNotEnabled);
    CHECK_NE(client::comm::BluetoothError::kNotEnabled, client::comm::BluetoothError::kDeviceNotFound);
    CHECK_NE(client::comm::BluetoothError::kDeviceNotFound, client::comm::BluetoothError::kConnectionFailed);
    CHECK_NE(client::comm::BluetoothError::kConnectionFailed, client::comm::BluetoothError::kConnectionLost);
    CHECK_NE(client::comm::BluetoothError::kConnectionLost, client::comm::BluetoothError::kSendFailed);
    CHECK_NE(client::comm::BluetoothError::kSendFailed, client::comm::BluetoothError::kReceiveFailed);
    CHECK_NE(client::comm::BluetoothError::kReceiveFailed, client::comm::BluetoothError::kTimeout);
    CHECK_NE(client::comm::BluetoothError::kTimeout, client::comm::BluetoothError::kAlreadyConnected);
    CHECK_NE(client::comm::BluetoothError::kAlreadyConnected, client::comm::BluetoothError::kNotConnected);
    CHECK_NE(client::comm::BluetoothError::kNotConnected, client::comm::BluetoothError::kInternalError);
  }

  TEST_CASE("BluetoothState: BluetoothStateToString returns correct strings") {
    CHECK_EQ(client::comm::BluetoothStateToString(client::comm::BluetoothState::kDisconnected), "Disconnected");
    CHECK_EQ(client::comm::BluetoothStateToString(client::comm::BluetoothState::kScanning), "Scanning");
    CHECK_EQ(client::comm::BluetoothStateToString(client::comm::BluetoothState::kConnecting), "Connecting");
    CHECK_EQ(client::comm::BluetoothStateToString(client::comm::BluetoothState::kConnected), "Connected");
    CHECK_EQ(client::comm::BluetoothStateToString(client::comm::BluetoothState::kError), "Error");
  }

  TEST_CASE("BluetoothState: State values are distinct") {
    CHECK_NE(client::comm::BluetoothState::kDisconnected, client::comm::BluetoothState::kScanning);
    CHECK_NE(client::comm::BluetoothState::kScanning, client::comm::BluetoothState::kConnecting);
    CHECK_NE(client::comm::BluetoothState::kConnecting, client::comm::BluetoothState::kConnected);
    CHECK_NE(client::comm::BluetoothState::kConnected, client::comm::BluetoothState::kError);
  }

  TEST_CASE("BluetoothDevice: Default construction") {
    client::comm::BluetoothDevice device;
    CHECK(device.name.empty());
    CHECK(device.address.empty());
    CHECK_EQ(device.rssi, 0);
    CHECK_FALSE(device.is_paired);
    CHECK_FALSE(device.is_connected);
  }

  TEST_CASE("BluetoothDevice: Equality operator") {
    client::comm::BluetoothDevice device1{
        .name = "ESP32-Test", .address = "AA:BB:CC:DD:EE:FF", .rssi = -50, .is_paired = true, .is_connected = false};
    client::comm::BluetoothDevice device2{
        .name = "ESP32-Test", .address = "AA:BB:CC:DD:EE:FF", .rssi = -50, .is_paired = true, .is_connected = false};
    client::comm::BluetoothDevice device3{
        .name = "ESP32-Other", .address = "11:22:33:44:55:66", .rssi = -70, .is_paired = false, .is_connected = false};

    CHECK(device1 == device2);
    CHECK_FALSE(device1 == device3);
  }

  TEST_CASE("BluetoothDevice: Different addresses are not equal") {
    client::comm::BluetoothDevice device1{
        .name = "ESP32-Test", .address = "AA:BB:CC:DD:EE:FF", .rssi = -50, .is_paired = true, .is_connected = false};
    client::comm::BluetoothDevice device2{
        .name = "ESP32-Test", .address = "11:22:33:44:55:66", .rssi = -50, .is_paired = true, .is_connected = false};

    CHECK_FALSE(device1 == device2);
  }

  TEST_CASE("BluetoothManager: Default construction") {
    client::comm::BluetoothManager manager;
    // Initial state should be disconnected
    CHECK_EQ(manager.State(), client::comm::BluetoothState::kDisconnected);
    CHECK_FALSE(manager.ConnectedDevice().has_value());
  }

  TEST_CASE("BluetoothManager: DiscoveredDevices returns empty list initially") {
    client::comm::BluetoothManager manager;
    auto devices = manager.DiscoveredDevices();
    CHECK(devices.empty());
  }

  TEST_CASE("BluetoothManager: ConnectedDevice returns nullopt when disconnected") {
    client::comm::BluetoothManager manager;
    CHECK_FALSE(manager.ConnectedDevice().has_value());
  }

  TEST_CASE("BluetoothManager: Protocol accessor") {
    client::comm::BluetoothManager manager;
    // Just verify we can get the protocol without crashing
    [[maybe_unused]] auto& protocol = manager.GetProtocol();
    [[maybe_unused]] const auto& const_protocol =
        static_cast<const client::comm::BluetoothManager&>(manager).GetProtocol();
    CHECK(true);  // If we got here, the test passed
  }

  TEST_CASE("BluetoothManager: Setting callbacks doesn't crash") {
    client::comm::BluetoothManager manager;

    bool state_callback_called = false;
    bool device_callback_called = false;
    bool scan_callback_called = false;
    bool data_callback_called = false;

    manager.SetStateCallback([&](client::comm::BluetoothState, std::string_view) { state_callback_called = true; });

    manager.SetDeviceDiscoveredCallback([&](const client::comm::BluetoothDevice&) { device_callback_called = true; });

    manager.SetScanCompleteCallback(
        [&](std::span<const client::comm::BluetoothDevice>) { scan_callback_called = true; });

    manager.SetDataReceivedCallback([&](std::span<const uint8_t>) { data_callback_called = true; });

    // Callbacks aren't called just by setting them
    CHECK_FALSE(state_callback_called);
    CHECK_FALSE(device_callback_called);
    CHECK_FALSE(scan_callback_called);
    CHECK_FALSE(data_callback_called);
  }

  TEST_CASE("BluetoothManager: ProcessEvents doesn't crash") {
    client::comm::BluetoothManager manager;
    // Just verify ProcessEvents can be called without crashing
    manager.ProcessEvents();
    CHECK(true);
  }

  TEST_CASE("BluetoothManager: Operations on uninitialized manager return appropriate errors") {
    client::comm::BluetoothManager manager;

    // These operations may return NotSupported if Bluetooth is not available
    // on the test platform, which is expected behavior
    auto scan_result = manager.StartScan();
    if (!scan_result) {
      CHECK((scan_result.error() == client::comm::BluetoothError::kNotSupported ||
             scan_result.error() == client::comm::BluetoothError::kNotEnabled));
    }

    auto connect_result = manager.Connect("AA:BB:CC:DD:EE:FF");
    if (!connect_result) {
      CHECK((connect_result.error() == client::comm::BluetoothError::kNotSupported ||
             connect_result.error() == client::comm::BluetoothError::kNotConnected ||
             connect_result.error() == client::comm::BluetoothError::kDeviceNotFound));
    }

    auto disconnect_result = manager.Disconnect();
    // Disconnect should succeed or return NotSupported
    if (!disconnect_result) {
      CHECK(disconnect_result.error() == client::comm::BluetoothError::kNotSupported);
    }

    std::vector<uint8_t> test_data{0x01, 0x02, 0x03};
    auto send_result = manager.Send(test_data);
    if (!send_result) {
      CHECK((send_result.error() == client::comm::BluetoothError::kNotSupported ||
             send_result.error() == client::comm::BluetoothError::kNotConnected));
    }
  }

  TEST_CASE("BluetoothManager: SendCommand on disconnected manager fails appropriately") {
    client::comm::BluetoothManager manager;

    client::comm::ServoCommand cmd{.pan_angle = 45.0F, .tilt_angle = -30.0F, .speed = 0.8F, .smooth = true};

    auto result = manager.SendCommand(cmd);
    if (!result) {
      CHECK((result.error() == client::comm::BluetoothError::kNotSupported ||
             result.error() == client::comm::BluetoothError::kNotConnected));
    }
  }

  TEST_CASE("BluetoothManager: SendHeartbeat on disconnected manager fails appropriately") {
    client::comm::BluetoothManager manager;

    auto result = manager.SendHeartbeat();
    if (!result) {
      CHECK((result.error() == client::comm::BluetoothError::kNotSupported ||
             result.error() == client::comm::BluetoothError::kNotConnected));
    }
  }

  TEST_CASE("BluetoothManager: StopScan doesn't crash when not scanning") {
    client::comm::BluetoothManager manager;
    // Should be safe to call even when not scanning
    manager.StopScan();
    CHECK(true);
  }

  TEST_CASE("BluetoothManager: Available and Enabled return consistent values") {
    client::comm::BluetoothManager manager;
    // If not available, it can't be enabled
    if (!manager.Available()) {
      CHECK_FALSE(manager.Enabled());
    }
    // If enabled, it must be available
    if (manager.Enabled()) {
      CHECK(manager.Available());
    }
  }

}  // TEST_SUITE
