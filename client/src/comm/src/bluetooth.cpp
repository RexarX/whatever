#include <client/comm/bluetooth.hpp>

#include <client/core/logger.hpp>

#include <atomic>
#include <bit>
#include <chrono>
#include <expected>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#ifdef CLIENT_COMM_HAS_BLUETOOTH

#include <QBluetoothDeviceDiscoveryAgent>
#include <QBluetoothDeviceInfo>
#include <QBluetoothLocalDevice>
#include <QBluetoothServiceDiscoveryAgent>
#include <QBluetoothSocket>
#include <QBluetoothUuid>
#include <QCoreApplication>
#include <QObject>
#include <QTimer>

#endif  // CLIENT_COMM_HAS_BLUETOOTH

namespace client::comm {

#ifdef CLIENT_COMM_HAS_BLUETOOTH

namespace {

/// ESP32 SPP UUID for serial communication.
constexpr const char* kSerialPortServiceUuid = "00001101-0000-1000-8000-00805F9B34FB";

}  // namespace

/**
 * @brief Qt-based Bluetooth implementation.
 */
class BluetoothManagerQt : public QObject {
  Q_OBJECT

public:
  explicit BluetoothManagerQt(QObject* parent = nullptr) : QObject(parent) {}

  ~BluetoothManagerQt() override {
    if (socket_ && socket_->state() == QBluetoothSocket::SocketState::ConnectedState) {
      socket_->disconnectFromService();
    }
  }

  BluetoothManagerQt(const BluetoothManagerQt&) = delete;
  BluetoothManagerQt(BluetoothManagerQt&&) = delete;

  BluetoothManagerQt& operator=(const BluetoothManagerQt&) = delete;
  BluetoothManagerQt& operator=(BluetoothManagerQt&&) = delete;

  auto Initialize() -> std::expected<void, BluetoothError>;

  auto StartScan(uint32_t timeout_ms) -> std::expected<void, BluetoothError>;
  void StopScan();

  auto Connect(std::string_view address) -> std::expected<void, BluetoothError>;
  auto Disconnect() -> std::expected<void, BluetoothError>;

  auto Send(std::span<const uint8_t> data) -> std::expected<size_t, BluetoothError>;

  void SetStateCallback(BluetoothManager::StateCallback callback) noexcept { state_callback_ = std::move(callback); }

  void SetDeviceDiscoveredCallback(BluetoothManager::DeviceDiscoveredCallback callback) noexcept {
    device_discovered_callback_ = std::move(callback);
  }

  void SetScanCompleteCallback(BluetoothManager::ScanCompleteCallback callback) noexcept {
    scan_complete_callback_ = std::move(callback);
  }

  void SetDataReceivedCallback(BluetoothManager::DataReceivedCallback callback) noexcept {
    data_received_callback_ = std::move(callback);
  }

  [[nodiscard]] bool Available() const noexcept { return local_device_ && local_device_->isValid(); }

  [[nodiscard]] bool Enabled() const noexcept;

  [[nodiscard]] auto DiscoveredDevices() const -> std::vector<BluetoothDevice>;

  [[nodiscard]] BluetoothState State() const noexcept { return state_.load(std::memory_order_relaxed); }

  [[nodiscard]] auto ConnectedDevice() const -> std::optional<BluetoothDevice>;

  [[nodiscard]] std::string_view LastError() const noexcept { return last_error_; }

  [[nodiscard]] Protocol& GetProtocol() noexcept { return protocol_; }
  [[nodiscard]] const Protocol& GetProtocol() const noexcept { return protocol_; }

private slots:
  void OnDeviceDiscovered(const QBluetoothDeviceInfo& info);
  void OnScanFinished();
  void OnScanError(QBluetoothDeviceDiscoveryAgent::Error error);
  void OnSocketConnected();
  void OnSocketDisconnected();
  void OnSocketError(QBluetoothSocket::SocketError error);
  void OnSocketReadyRead();

private:
  void SetState(BluetoothState state, std::string_view error_message = "");

  Protocol protocol_;
  std::unique_ptr<QBluetoothLocalDevice> local_device_;
  std::unique_ptr<QBluetoothDeviceDiscoveryAgent> discovery_agent_;
  std::unique_ptr<QBluetoothSocket> socket_;

  mutable std::shared_mutex mutex_;
  std::vector<BluetoothDevice> discovered_devices_;
  std::optional<BluetoothDevice> connected_device_;
  std::atomic<BluetoothState> state_{BluetoothState::kDisconnected};
  std::string last_error_;
  bool initialized_ = false;

  BluetoothManager::StateCallback state_callback_;
  BluetoothManager::DeviceDiscoveredCallback device_discovered_callback_;
  BluetoothManager::ScanCompleteCallback scan_complete_callback_;
  BluetoothManager::DataReceivedCallback data_received_callback_;
};

auto BluetoothManagerQt::Initialize() -> std::expected<void, BluetoothError> {
  if (initialized_) {
    return {};
  }

  local_device_ = std::make_unique<QBluetoothLocalDevice>(this);
  if (!local_device_->isValid()) {
    last_error_ = "Bluetooth adapter not available";
    return std::unexpected(BluetoothError::kNotSupported);
  }

  discovery_agent_ = std::make_unique<QBluetoothDeviceDiscoveryAgent>(this);
  discovery_agent_->setLowEnergyDiscoveryTimeout(5000);

  connect(discovery_agent_.get(), &QBluetoothDeviceDiscoveryAgent::deviceDiscovered, this,
          &BluetoothManagerQt::OnDeviceDiscovered);
  connect(discovery_agent_.get(), &QBluetoothDeviceDiscoveryAgent::finished, this, &BluetoothManagerQt::OnScanFinished);
  connect(discovery_agent_.get(), &QBluetoothDeviceDiscoveryAgent::errorOccurred, this,
          &BluetoothManagerQt::OnScanError);

  initialized_ = true;
  return {};
}

auto BluetoothManagerQt::StartScan(uint32_t timeout_ms) -> std::expected<void, BluetoothError> {
  if (!Available()) {
    CLIENT_WARN("Bluetooth not available on this system");
    return std::unexpected(BluetoothError::kNotSupported);
  }

  if (!Enabled()) {
    CLIENT_WARN("Bluetooth is not enabled");
    return std::unexpected(BluetoothError::kNotEnabled);
  }

  {
    std::scoped_lock lock(mutex_);
    discovered_devices_.clear();

    // Add already paired devices to the list
    if (local_device_ && local_device_->isValid()) {
      const auto paired_devices = local_device_->connectedDevices();
      CLIENT_INFO("Found {} already connected device(s)", paired_devices.size());

      for (const auto& addr : paired_devices) {
        QBluetoothDeviceInfo info(addr, "", QBluetoothDeviceInfo::MiscellaneousDevice);
        BluetoothDevice device{.name = "Connected Device",
                               .address = addr.toString().toStdString(),
                               .rssi = 0,
                               .is_paired = true,
                               .is_connected = true};
        discovered_devices_.push_back(device);
        CLIENT_INFO("Added connected device: {}", addr.toString().toStdString());
      }
    }
  }

  SetState(BluetoothState::kScanning);

  if (timeout_ms > 0) {
    discovery_agent_->setLowEnergyDiscoveryTimeout(static_cast<int>(timeout_ms));
  }

  CLIENT_INFO("Starting Bluetooth scan for classic devices (timeout: {} ms)", timeout_ms);
  discovery_agent_->start(QBluetoothDeviceDiscoveryAgent::ClassicMethod);
  return {};
}

void BluetoothManagerQt::StopScan() {
  if (discovery_agent_ && discovery_agent_->isActive()) {
    discovery_agent_->stop();
  }
  if (state_.load(std::memory_order_relaxed) == BluetoothState::kScanning) {
    SetState(BluetoothState::kDisconnected);
  }
}

auto BluetoothManagerQt::Connect(std::string_view address) -> std::expected<void, BluetoothError> {
  if (!Available()) {
    return std::unexpected(BluetoothError::kNotSupported);
  }

  if (state_.load(std::memory_order_relaxed) == BluetoothState::kConnected) {
    return std::unexpected(BluetoothError::kAlreadyConnected);
  }

  StopScan();

  const auto addr_str = QString::fromUtf8(address.data(), static_cast<qsizetype>(address.size()));
  const QBluetoothAddress bt_address(addr_str);

  if (bt_address.isNull()) {
    last_error_ = "Invalid Bluetooth address";
    CLIENT_ERROR("Invalid Bluetooth address: {}", addr_str.toStdString());
    return std::unexpected(BluetoothError::kDeviceNotFound);
  }

  CLIENT_INFO("Attempting to connect to Bluetooth device: {} using SPP service UUID: {}", addr_str.toStdString(),
              kSerialPortServiceUuid);

  socket_ = std::make_unique<QBluetoothSocket>(QBluetoothServiceInfo::RfcommProtocol, this);

  connect(socket_.get(), &QBluetoothSocket::connected, this, &BluetoothManagerQt::OnSocketConnected);
  connect(socket_.get(), &QBluetoothSocket::disconnected, this, &BluetoothManagerQt::OnSocketDisconnected);
  connect(socket_.get(), &QBluetoothSocket::errorOccurred, this, &BluetoothManagerQt::OnSocketError);
  connect(socket_.get(), &QBluetoothSocket::readyRead, this, &BluetoothManagerQt::OnSocketReadyRead);

  SetState(BluetoothState::kConnecting);

  // Connect to SPP service
  const QBluetoothUuid service_uuid(QString::fromLatin1(kSerialPortServiceUuid));
  socket_->connectToService(bt_address, service_uuid);

  // Store device info
  {
    std::scoped_lock lock(mutex_);
    connected_device_ = BluetoothDevice{
        .name = "ESP32 Device", .address = std::string(address), .rssi = 0, .is_paired = false, .is_connected = false};
  }

  return {};
}

auto BluetoothManagerQt::Disconnect() -> std::expected<void, BluetoothError> {
  if (state_.load(std::memory_order_relaxed) == BluetoothState::kDisconnected) {
    return {};
  }

  if (socket_) {
    socket_->disconnectFromService();
  }

  return {};
}

auto BluetoothManagerQt::Send(std::span<const uint8_t> data) -> std::expected<size_t, BluetoothError> {
  if (state_.load(std::memory_order_relaxed) != BluetoothState::kConnected) {
    return std::unexpected(BluetoothError::kNotConnected);
  }

  if (!socket_ || socket_->state() != QBluetoothSocket::SocketState::ConnectedState) {
    return std::unexpected(BluetoothError::kNotConnected);
  }

  const auto bytes_written = socket_->write(std::bit_cast<const char*>(data.data()), static_cast<qint64>(data.size()));

  if (bytes_written < 0) {
    last_error_ = socket_->errorString().toStdString();
    return std::unexpected(BluetoothError::kSendFailed);
  }

  return static_cast<size_t>(bytes_written);
}

bool BluetoothManagerQt::Enabled() const noexcept {
  if (!local_device_ || !local_device_->isValid()) {
    return false;
  }
  return local_device_->hostMode() != QBluetoothLocalDevice::HostPoweredOff;
}

auto BluetoothManagerQt::DiscoveredDevices() const -> std::vector<BluetoothDevice> {
  std::shared_lock lock(mutex_);
  return discovered_devices_;
}

auto BluetoothManagerQt::ConnectedDevice() const -> std::optional<BluetoothDevice> {
  std::shared_lock lock(mutex_);
  return connected_device_;
}

void BluetoothManagerQt::OnDeviceDiscovered(const QBluetoothDeviceInfo& info) {
  BluetoothDevice device{
      .name = info.name().toStdString(),
      .address = info.address().toString().toStdString(),
      .rssi = info.rssi(),
      .is_paired = local_device_ && local_device_->pairingStatus(info.address()) != QBluetoothLocalDevice::Unpaired,
      .is_connected = false};

  // Log discovered device for debugging
  CLIENT_INFO("Discovered Bluetooth device: {} ({}) RSSI: {} dBm, Paired: {}", device.name, device.address, device.rssi,
              device.is_paired);

  {
    std::scoped_lock lock(mutex_);
    const auto it = std::ranges::find_if(discovered_devices_,
                                         [&device](const BluetoothDevice& d) { return d.address == device.address; });

    if (it == discovered_devices_.end()) {
      discovered_devices_.push_back(device);
    } else {
      *it = device;
    }
  }

  if (device_discovered_callback_) {
    device_discovered_callback_(device);
  }
}

void BluetoothManagerQt::OnScanFinished() {
  if (state_.load(std::memory_order_relaxed) == BluetoothState::kScanning) {
    SetState(BluetoothState::kDisconnected);
  }

  if (scan_complete_callback_) {
    std::shared_lock lock(mutex_);
    scan_complete_callback_(discovered_devices_);
  }
}

void BluetoothManagerQt::OnScanError(QBluetoothDeviceDiscoveryAgent::Error error) {
  std::string_view error_msg;
  switch (error) {
    case QBluetoothDeviceDiscoveryAgent::PoweredOffError:
      error_msg = "Bluetooth is powered off";
      break;
    case QBluetoothDeviceDiscoveryAgent::InputOutputError:
      error_msg = "I/O error during discovery";
      break;
    case QBluetoothDeviceDiscoveryAgent::InvalidBluetoothAdapterError:
      error_msg = "Invalid Bluetooth adapter";
      break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedPlatformError:
      error_msg = "Platform does not support Bluetooth discovery";
      break;
    case QBluetoothDeviceDiscoveryAgent::UnsupportedDiscoveryMethod:
      error_msg = "Unsupported discovery method";
      break;
    default:
      error_msg = "Unknown discovery error";
      break;
  }

  last_error_ = error_msg;
  SetState(BluetoothState::kError, error_msg);
}

void BluetoothManagerQt::OnSocketConnected() {
  {
    std::scoped_lock lock(mutex_);
    if (connected_device_) {
      connected_device_->is_connected = true;
      CLIENT_INFO("Successfully connected to Bluetooth device: {} ({})", connected_device_->name,
                  connected_device_->address);
    }
  }
  SetState(BluetoothState::kConnected);
}

void BluetoothManagerQt::OnSocketDisconnected() {
  {
    std::scoped_lock lock(mutex_);
    connected_device_.reset();
  }

  SetState(BluetoothState::kDisconnected);
}

void BluetoothManagerQt::OnSocketError(QBluetoothSocket::SocketError error) {
  std::string error_msg;
  switch (error) {
    case QBluetoothSocket::SocketError::UnknownSocketError:
      error_msg = "Unknown socket error";
      break;
    case QBluetoothSocket::SocketError::NoSocketError:
      return;  // No error
    case QBluetoothSocket::SocketError::HostNotFoundError:
      error_msg = "Device not found";
      break;
    case QBluetoothSocket::SocketError::ServiceNotFoundError:
      error_msg = "Service not found on device";
      break;
    case QBluetoothSocket::SocketError::NetworkError:
      error_msg = "Network error";
      break;
    case QBluetoothSocket::SocketError::UnsupportedProtocolError:
      error_msg = "Unsupported protocol";
      break;
    case QBluetoothSocket::SocketError::OperationError:
      error_msg = "Operation error";
      break;
    case QBluetoothSocket::SocketError::RemoteHostClosedError:
      error_msg = "Connection closed by remote device";
      break;
    default:
      error_msg = socket_ ? socket_->errorString().toStdString() : "Unknown error";
      break;
  }

  CLIENT_ERROR("Bluetooth socket error: {}", error_msg);
  if (socket_) {
    CLIENT_ERROR("Socket state: {}, Error code: {}", static_cast<int>(socket_->state()), static_cast<int>(error));
  }

  last_error_ = error_msg;

  {
    std::scoped_lock lock(mutex_);
    connected_device_.reset();
  }

  SetState(BluetoothState::kError, error_msg);
}

void BluetoothManagerQt::OnSocketReadyRead() {
  if (!socket_ || !data_received_callback_) {
    return;
  }

  const auto data = socket_->readAll();
  if (!data.isEmpty()) {
    const auto* data_ptr = std::bit_cast<const uint8_t*>(data.constData());
    data_received_callback_(std::span<const uint8_t>(data_ptr, static_cast<size_t>(data.size())));
  }
}

void BluetoothManagerQt::SetState(BluetoothState state, std::string_view error_message) {
  const auto old_state = state_.exchange(state, std::memory_order_relaxed);
  if (!error_message.empty()) {
    last_error_ = std::string(error_message);
  }

  if (old_state != state && state_callback_) {
    state_callback_(state, error_message);
  }
}

#include "bluetooth.moc"

#endif  // CLIENT_COMM_HAS_BLUETOOTH

struct BluetoothManager::Impl {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  BluetoothManagerQt qt_impl;
#else
  Protocol protocol;
  std::atomic<BluetoothState> state{BluetoothState::kDisconnected};
  std::string last_error = "Bluetooth not supported on this platform";
#endif

  Impl() = default;
  ~Impl() = default;

  // Non-copyable and non-moveable (QObject-derived types can't be moved)
  Impl(const Impl&) = delete;
  Impl& operator=(const Impl&) = delete;
  Impl(Impl&&) = delete;
  Impl& operator=(Impl&&) = delete;
};

BluetoothManager::BluetoothManager() = default;
BluetoothManager::~BluetoothManager() = default;

auto BluetoothManager::Initialize() -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Initialize();
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

bool BluetoothManager::Available() const noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Available();
#else
  return false;
#endif
}

bool BluetoothManager::Enabled() const noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Enabled();
#else
  return false;
#endif
}

auto BluetoothManager::StartScan([[maybe_unused]] uint32_t timeout_ms) -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.StartScan(timeout_ms);
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

void BluetoothManager::StopScan() {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  impl_->qt_impl.StopScan();
#endif
}

auto BluetoothManager::DiscoveredDevices() const -> std::vector<BluetoothDevice> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.DiscoveredDevices();
#else
  return {};
#endif
}

auto BluetoothManager::Connect([[maybe_unused]] std::string_view address) -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Connect(address);
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::Disconnect() -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Disconnect();
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::Send([[maybe_unused]] std::span<const uint8_t> data) -> std::expected<size_t, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.Send(data);
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::SendCommand([[maybe_unused]] const ServoCommand& cmd) -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  auto serialized = impl_->qt_impl.GetProtocol().SerializeServoCommand(cmd);
  if (!serialized) {
    return std::unexpected(BluetoothError::kSendFailed);
  }

  const auto result = impl_->qt_impl.Send(*serialized);
  if (!result) {
    return std::unexpected(result.error());
  }

  return {};
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::SendHeartbeat() -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  HeartbeatMessage msg{.timestamp_ms = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                 std::chrono::steady_clock::now().time_since_epoch())
                                                                 .count()),
                       .sequence = 0};

  auto serialized = impl_->qt_impl.GetProtocol().SerializeHeartbeat(msg);
  if (!serialized) {
    return std::unexpected(BluetoothError::kSendFailed);
  }

  const auto result = impl_->qt_impl.Send(*serialized);
  if (!result) {
    return std::unexpected(result.error());
  }

  return {};
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::SendCalibrate() -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  auto serialized = impl_->qt_impl.GetProtocol().SerializeCalibrate();
  if (!serialized) {
    return std::unexpected(BluetoothError::kSendFailed);
  }

  const auto result = impl_->qt_impl.Send(*serialized);
  if (!result) {
    return std::unexpected(result.error());
  }

  return {};
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

auto BluetoothManager::SendHome() -> std::expected<void, BluetoothError> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  auto serialized = impl_->qt_impl.GetProtocol().SerializeHome();
  if (!serialized) {
    return std::unexpected(BluetoothError::kSendFailed);
  }

  const auto result = impl_->qt_impl.Send(*serialized);
  if (!result) {
    return std::unexpected(result.error());
  }

  return {};
#else
  return std::unexpected(BluetoothError::kNotSupported);
#endif
}

void BluetoothManager::SetStateCallback([[maybe_unused]] StateCallback callback) noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  impl_->qt_impl.SetStateCallback(std::move(callback));
#endif
}

void BluetoothManager::SetDeviceDiscoveredCallback([[maybe_unused]] DeviceDiscoveredCallback callback) noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  impl_->qt_impl.SetDeviceDiscoveredCallback(std::move(callback));
#endif
}

void BluetoothManager::SetScanCompleteCallback([[maybe_unused]] ScanCompleteCallback callback) noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  impl_->qt_impl.SetScanCompleteCallback(std::move(callback));
#endif
}

void BluetoothManager::SetDataReceivedCallback([[maybe_unused]] DataReceivedCallback callback) noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  impl_->qt_impl.SetDataReceivedCallback(std::move(callback));
#endif
}

BluetoothState BluetoothManager::State() const noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.State();
#else
  return impl_->state.load(std::memory_order_relaxed);
#endif
}

auto BluetoothManager::ConnectedDevice() const -> std::optional<BluetoothDevice> {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.ConnectedDevice();
#else
  return std::nullopt;
#endif
}

std::string_view BluetoothManager::LastError() const noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.LastError();
#else
  return impl_->last_error;
#endif
}

Protocol& BluetoothManager::GetProtocol() noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.GetProtocol();
#else
  return impl_->protocol;
#endif
}

const Protocol& BluetoothManager::GetProtocol() const noexcept {
#ifdef CLIENT_COMM_HAS_BLUETOOTH
  return impl_->qt_impl.GetProtocol();
#else
  return impl_->protocol;
#endif
}

}  // namespace client::comm
