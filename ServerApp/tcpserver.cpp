#include "tcpserver.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QMutexLocker>

namespace {
// Разделитель сообщений для TCP потока (протокол на основе переноса строки)
constexpr char kMessageDelimiter = '\n';
}  // namespace

TcpServer::TcpServer(QObject *parent)
    : QObject(parent),
      server_(new QTcpServer(this)),
      next_client_id_(1)
{
    // Регистрация метатипов для передачи через сигналы между потоками
    qRegisterMetaType<ClientInfo>("ClientInfo");
    qRegisterMetaType<ClientData>("ClientData");
    qRegisterMetaType<ThresholdConfig>("ThresholdConfig");

    connect(server_, &QTcpServer::newConnection,
            this, &TcpServer::onNewConnection);
}

TcpServer::~TcpServer()
{
    stopServer();
}

bool TcpServer::startServer(quint16 port)
{
    if (server_->isListening()) {
        emit logMessage("Server is already running");
        return true;
    }

    if (!server_->listen(QHostAddress::Any, port)) {
        emit logMessage(QString("Failed to start server: %1")
                        .arg(server_->errorString()));
        return false;
    }

    emit logMessage(QString("Server started on port %1").arg(port));
    emit serverStarted();
    return true;
}

void TcpServer::stopServer()
{
    // Отключаем всех клиентов (abort для немедленного закрытия)
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        QTcpSocket *socket = it.key();
        socket->abort();
        socket->deleteLater();
    }
    clients_.clear();
    client_sockets_.clear();
    receive_buffers_.clear();

    if (server_->isListening()) {
        server_->close();
        emit logMessage("Server stopped");
        emit serverStopped();
    }
}

bool TcpServer::isRunning() const
{
    return server_->isListening();
}

void TcpServer::startAllClients()
{
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (it.value().is_connected && !it.value().is_running) {
            startClient(it.value().id);
        }
    }
}

void TcpServer::stopAllClients()
{
    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        if (it.value().is_running) {
            stopClient(it.value().id);
        }
    }
}

void TcpServer::startClient(int client_id)
{
    if (!client_sockets_.contains(client_id)) {
        emit logMessage(QString("Client %1 not found").arg(client_id));
        return;
    }

    QTcpSocket *socket = client_sockets_[client_id];
    clients_[socket].is_running = true;

    // Отправляем команду start клиенту
    QJsonObject command;
    command["type"] = "Command";
    command["command"] = "start";
    sendToClient(socket, command);

    emit clientStatusChanged(client_id, true);
    emit logMessage(QString("Started client %1").arg(client_id));
}

void TcpServer::stopClient(int client_id)
{
    if (!client_sockets_.contains(client_id)) {
        emit logMessage(QString("Client %1 not found").arg(client_id));
        return;
    }

    QTcpSocket *socket = client_sockets_[client_id];
    clients_[socket].is_running = false;

    // Отправляем команду stop клиенту
    QJsonObject command;
    command["type"] = "Command";
    command["command"] = "stop";
    sendToClient(socket, command);

    emit clientStatusChanged(client_id, false);
    emit logMessage(QString("Stopped client %1").arg(client_id));
}

void TcpServer::setThresholds(const ThresholdConfig &config)
{
    QMutexLocker locker(&thresholds_mutex_);
    thresholds_ = config;
}

ThresholdConfig TcpServer::getThresholds() const
{
    QMutexLocker locker(&thresholds_mutex_);
    return thresholds_;
}

void TcpServer::onNewConnection()
{
    while (server_->hasPendingConnections()) {
        QTcpSocket *socket = server_->nextPendingConnection();

        // Set up signal connections
        connect(socket, &QTcpSocket::disconnected,
                this, &TcpServer::onClientDisconnected);
        connect(socket, &QTcpSocket::readyRead,
                this, &TcpServer::onReadyRead);
        connect(socket, &QTcpSocket::errorOccurred,
                this, &TcpServer::onSocketError);

        // Create client info
        ClientInfo info;
        info.id = generateClientId();
        info.ip_address = socket->peerAddress().toString();
        info.port = socket->peerPort();
        info.is_connected = true;
        info.is_running = false;

        // Store client
        clients_[socket] = info;
        client_sockets_[info.id] = socket;
        receive_buffers_[socket] = QByteArray();

        // Send connection confirmation
        QJsonObject confirmation;
        confirmation["type"] = "ConnectionConfirm";
        confirmation["client_id"] = info.id;
        confirmation["status"] = "connected";
        sendToClient(socket, confirmation);

        emit clientConnected(info);
        emit logMessage(QString("Client %1 connected from %2:%3")
                        .arg(info.id)
                        .arg(info.ip_address)
                        .arg(info.port));
    }
}

void TcpServer::onClientDisconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !clients_.contains(socket)) {
        return;
    }

    int client_id = clients_[socket].id;
    emit clientDisconnected(client_id);
    emit logMessage(QString("Client %1 disconnected").arg(client_id));

    // Clean up
    client_sockets_.remove(client_id);
    clients_.remove(socket);
    receive_buffers_.remove(socket);
    socket->deleteLater();
}

void TcpServer::onReadyRead()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !clients_.contains(socket)) {
        return;
    }

    int client_id = clients_[socket].id;

    // Добавляем данные в буфер
    receive_buffers_[socket].append(socket->readAll());

    // Защита от переполнения буфера
    if (receive_buffers_[socket].size() > kMaxBufferSize) {
        emit logMessage(QString("Client %1: buffer overflow, disconnecting").arg(client_id));
        socket->abort();
        return;
    }

    // Обрабатываем полные сообщения (разделенные переносом строки)
    while (receive_buffers_[socket].contains(kMessageDelimiter)) {
        int delimiter_pos = receive_buffers_[socket].indexOf(kMessageDelimiter);
        QByteArray message = receive_buffers_[socket].left(delimiter_pos);
        receive_buffers_[socket].remove(0, delimiter_pos + 1);

        if (!message.isEmpty()) {
            processClientData(client_id, message);
        }
    }
}

void TcpServer::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    QTcpSocket *socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) {
        return;
    }

    emit logMessage(QString("Socket error: %1").arg(socket->errorString()));
}

void TcpServer::sendToClient(QTcpSocket *socket, const QJsonObject &message)
{
    if (!socket || socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QJsonDocument doc(message);
    QByteArray data = doc.toJson(QJsonDocument::Compact);
    data.append(kMessageDelimiter);

    qint64 bytes_written = socket->write(data);
    if (bytes_written == -1) {
        emit logMessage(QString("Write error: %1").arg(socket->errorString()));
    } else if (bytes_written != data.size()) {
        emit logMessage(QString("Partial write: %1/%2 bytes").arg(bytes_written).arg(data.size()));
    }
}

void TcpServer::processClientData(int client_id, const QByteArray &data)
{
    QJsonParseError parse_error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &parse_error);

    if (parse_error.error != QJsonParseError::NoError) {
        emit logMessage(QString("JSON parse error from client %1: %2")
                        .arg(client_id)
                        .arg(parse_error.errorString()));
        return;
    }

    if (!doc.isObject()) {
        emit logMessage(QString("Invalid JSON from client %1: not an object")
                        .arg(client_id));
        return;
    }

    QJsonObject obj = doc.object();
    QString type = obj["type"].toString();

    // Create data structure
    ClientData client_data;
    client_data.client_id = client_id;
    client_data.data_type = type;
    client_data.content = obj;
    client_data.timestamp = QDateTime::currentDateTime();

    emit dataReceived(client_data);

    // Check thresholds for warnings
    checkThresholds(client_id, obj);
}

void TcpServer::checkThresholds(int client_id, const QJsonObject &data)
{
    // Получаем копию настроек потокобезопасно
    ThresholdConfig config = getThresholds();

    QString type = data["type"].toString();
    QStringList warnings;

    if (type == "NetworkMetrics") {
        if (data.contains("latency")) {
            double latency = data["latency"].toDouble();
            if (latency > config.max_latency) {
                warnings << QString("High latency: %1ms").arg(latency);
            }
        }
        if (data.contains("packet_loss")) {
            double packet_loss = data["packet_loss"].toDouble();
            if (packet_loss > config.max_packet_loss) {
                warnings << QString("High packet loss: %1%").arg(packet_loss);
            }
        }
    } else if (type == "DeviceStatus") {
        if (data.contains("cpu_usage")) {
            int cpu_usage = data["cpu_usage"].toInt();
            if (cpu_usage > config.max_cpu_usage) {
                warnings << QString("High CPU usage: %1%").arg(cpu_usage);
            }
        }
        if (data.contains("memory_usage")) {
            int memory_usage = data["memory_usage"].toInt();
            if (memory_usage > config.max_memory_usage) {
                warnings << QString("High memory usage: %1%").arg(memory_usage);
            }
        }
    }

    // Логируем предупреждения
    for (const QString &warning : warnings) {
        emit logMessage(QString("WARNING [Client %1]: %2")
                        .arg(client_id).arg(warning));
    }
}

int TcpServer::generateClientId()
{
    return next_client_id_++;
}
