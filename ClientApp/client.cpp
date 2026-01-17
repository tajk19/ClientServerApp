#include "client.h"

#include <QJsonDocument>
#include <QRandomGenerator>
#include <QDateTime>

namespace {
constexpr char kMessageDelimiter = '\n';
constexpr int kReconnectIntervalMs = 5000;  // 5 seconds
constexpr int kMinSendIntervalMs = 10;      // 0.01 seconds
constexpr int kMaxSendIntervalMs = 100;     // 0.1 seconds

// Log message templates for variety
const QStringList kLogMessages = {
    "Interface eth0 restarted",
    "Connection established to gateway",
    "Packet buffer cleared",
    "Routing table updated",
    "DNS resolution completed",
    "Firewall rules reloaded",
    "Network interface configured",
    "DHCP lease renewed",
    "ARP cache flushed",
    "TCP connection timeout handled"
};

const QStringList kSeverities = {"INFO", "WARNING", "ERROR", "DEBUG"};
}  // namespace

Client::Client(QObject *parent)
    : QObject(parent),
      socket_(new QTcpSocket(this)),
      reconnect_timer_(new QTimer(this)),
      send_timer_(new QTimer(this)),
      port_(12345),
      client_id_(-1),
      state_(ClientState::Disconnected),
      uptime_(0),
      message_counter_(0)
{
    // Socket connections
    connect(socket_, &QTcpSocket::connected,
            this, &Client::onConnected);
    connect(socket_, &QTcpSocket::disconnected,
            this, &Client::onDisconnected);
    connect(socket_, &QTcpSocket::readyRead,
            this, &Client::onReadyRead);
    connect(socket_, &QTcpSocket::errorOccurred,
            this, &Client::onSocketError);

    // Timer connections
    connect(reconnect_timer_, &QTimer::timeout,
            this, &Client::onReconnectTimer);
    connect(send_timer_, &QTimer::timeout,
            this, &Client::onSendDataTimer);

    reconnect_timer_->setSingleShot(true);
    send_timer_->setSingleShot(true);
}

Client::~Client()
{
    disconnect();
}

void Client::connectToServer(const QString &host, quint16 port)
{
    host_ = host;
    port_ = port;

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }

    setState(ClientState::Connecting);
    emit logMessage(QString("Connecting to %1:%2...").arg(host_).arg(port_));
    socket_->connectToHost(host_, port_);
}

void Client::disconnect()
{
    reconnect_timer_->stop();
    send_timer_->stop();

    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->disconnectFromHost();
    }

    setState(ClientState::Disconnected);
}

ClientState Client::state() const
{
    return state_;
}

int Client::clientId() const
{
    return client_id_;
}

void Client::onConnected()
{
    emit logMessage("Connected to server, waiting for confirmation...");
    setState(ClientState::WaitingConfirmation);
    emit connected();
}

void Client::onDisconnected()
{
    emit logMessage("Disconnected from server");
    send_timer_->stop();
    client_id_ = -1;

    if (state_ != ClientState::Disconnected) {
        // Auto-reconnect
        emit logMessage(QString("Reconnecting in %1 seconds...")
                        .arg(kReconnectIntervalMs / 1000));
        reconnect_timer_->start(kReconnectIntervalMs);
    }

    emit disconnected();
}

void Client::onReadyRead()
{
    receive_buffer_.append(socket_->readAll());

    while (receive_buffer_.contains(kMessageDelimiter)) {
        int delimiter_pos = receive_buffer_.indexOf(kMessageDelimiter);
        QByteArray message = receive_buffer_.left(delimiter_pos);
        receive_buffer_.remove(0, delimiter_pos + 1);

        if (!message.isEmpty()) {
            processServerMessage(message);
        }
    }
}

void Client::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);

    if (state_ == ClientState::Connecting) {
        emit logMessage(QString("Connection failed: %1").arg(socket_->errorString()));
        emit logMessage(QString("Retrying in %1 seconds...")
                        .arg(kReconnectIntervalMs / 1000));
        reconnect_timer_->start(kReconnectIntervalMs);
    } else {
        emit logMessage(QString("Socket error: %1").arg(socket_->errorString()));
    }
}

void Client::onReconnectTimer()
{
    if (state_ != ClientState::Disconnected) {
        connectToServer(host_, port_);
    }
}

void Client::onSendDataTimer()
{
    if (state_ != ClientState::Running) {
        return;
    }

    // Rotate through data types
    QJsonObject data;
    int data_type = message_counter_ % 3;
    message_counter_++;

    switch (data_type) {
        case 0:
            data = generateNetworkMetrics();
            break;
        case 1:
            data = generateDeviceStatus();
            break;
        case 2:
            data = generateLogMessage();
            break;
    }

    sendMessage(data);

    // Schedule next send with random delay
    scheduleNextSend();
}

void Client::sendMessage(const QJsonObject &message)
{
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append(kMessageDelimiter);
    socket_->write(data);
}

void Client::processServerMessage(const QByteArray &data)
{
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);

    if (parse_error.error != QJsonParseError::NoError) {
        emit logMessage(QString("JSON parse error: %1").arg(parse_error.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    if (type == "ConnectionConfirm") {
        client_id_ = obj["client_id"].toInt();
        QString status = obj["status"].toString();
        emit logMessage(QString("Connection confirmed. Client ID: %1, Status: %2")
                        .arg(client_id_).arg(status));
        setState(ClientState::WaitingStart);
    } else if (type == "Command") {
        QString command = obj["command"].toString();
        if (command == "start") {
            emit logMessage("Received START command, beginning data transmission");
            setState(ClientState::Running);
            scheduleNextSend();
        } else if (command == "stop") {
            emit logMessage("Received STOP command, stopping data transmission");
            send_timer_->stop();
            setState(ClientState::Stopped);
        }
    } else {
        emit logMessage(QString("Unknown message type: %1").arg(type));
    }
}

QJsonObject Client::generateNetworkMetrics()
{
    auto randDouble = [](double min, double max) {
        return min + QRandomGenerator::global()->generateDouble() * (max - min);
    };

    QJsonObject metrics;
    metrics["type"] = "NetworkMetrics";
    metrics["bandwidth"] = randDouble(50.0, 150.0);
    metrics["latency"] = randDouble(1.0, 200.0);
    metrics["packet_loss"] = randDouble(0.0, 10.0);
    return metrics;
}

QJsonObject Client::generateDeviceStatus()
{
    // Simulate increasing uptime
    uptime_ += QRandomGenerator::global()->bounded(1, 60);

    QJsonObject status;
    status["type"] = "DeviceStatus";
    status["uptime"] = uptime_;
    status["cpu_usage"] = QRandomGenerator::global()->bounded(0, 100);
    status["memory_usage"] = QRandomGenerator::global()->bounded(20, 95);
    return status;
}

QJsonObject Client::generateLogMessage()
{
    // Select random log template
    int msg_idx = QRandomGenerator::global()->bounded(kLogMessages.size());
    int sev_idx = QRandomGenerator::global()->bounded(kSeverities.size());

    // Generate variable length message
    QString base_message = kLogMessages[msg_idx];
    QString extra = generateRandomString(0, 200);

    QJsonObject log;
    log["type"] = "Log";
    log["message"] = base_message + (extra.isEmpty() ? "" : " - " + extra);
    log["severity"] = kSeverities[sev_idx];
    return log;
}

QString Client::generateRandomString(int min_length, int max_length)
{
    int length = QRandomGenerator::global()->bounded(min_length, max_length + 1);
    if (length == 0) {
        return QString();
    }

    static const QString chars = "abcdefghijklmnopqrstuvwxyz0123456789 ";
    QString result;
    result.reserve(length);

    for (int i = 0; i < length; ++i) {
        int idx = QRandomGenerator::global()->bounded(chars.length());
        result.append(chars.at(idx));
    }

    return result;
}

void Client::setState(ClientState state)
{
    if (state_ != state) {
        state_ = state;
        emit stateChanged(state);
    }
}

void Client::scheduleNextSend()
{
    if (state_ != ClientState::Running) {
        return;
    }

    int delay = QRandomGenerator::global()->bounded(kMinSendIntervalMs, kMaxSendIntervalMs + 1);
    send_timer_->start(delay);
}
