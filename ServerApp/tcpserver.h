#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QDateTime>
#include <QJsonObject>
#include <QMutex>

// Structure to hold client information
struct ClientInfo {
    int id;
    QString ip_address;
    quint16 port;
    bool is_connected;
    bool is_running;  // Whether client is actively sending data
};

// Structure to hold received data
struct ClientData {
    int client_id;
    QString data_type;  // "NetworkMetrics", "DeviceStatus", "Log"
    QJsonObject content;
    QDateTime timestamp;
};

// Структура настроек пороговых значений
struct ThresholdConfig {
    double max_latency = 100.0;
    double max_packet_loss = 5.0;
    int max_cpu_usage = 90;
    int max_memory_usage = 90;
};

// Регистрация метатипов для передачи через сигналы между потоками
Q_DECLARE_METATYPE(ClientInfo)
Q_DECLARE_METATYPE(ClientData)
Q_DECLARE_METATYPE(ThresholdConfig)

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    bool isRunning() const;

    // Настройки (потокобезопасные)
    void setThresholds(const ThresholdConfig &config);
    ThresholdConfig getThresholds() const;

public slots:
    // Server control
    bool startServer(quint16 port = 12345);
    void stopServer();

    // Client management
    void startAllClients();
    void stopAllClients();
    void startClient(int client_id);
    void stopClient(int client_id);

signals:
    // Emitted when a new client connects
    void clientConnected(const ClientInfo &info);

    // Emitted when a client disconnects
    void clientDisconnected(int client_id);

    // Emitted when client status changes (start/stop)
    void clientStatusChanged(int client_id, bool is_running);

    // Emitted when data is received from a client
    void dataReceived(const ClientData &data);

    // Emitted for log messages
    void logMessage(const QString &message);

    // Emitted when server starts/stops
    void serverStarted();
    void serverStopped();

private slots:
    void onNewConnection();
    void onClientDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    // Send JSON message to a specific client
    void sendToClient(QTcpSocket *socket, const QJsonObject &message);

    // Process received JSON data
    void processClientData(int client_id, const QByteArray &data);

    // Check if data exceeds thresholds and generate warning
    void checkThresholds(int client_id, const QJsonObject &data);

    // Generate unique client ID
    int generateClientId();

    QTcpServer *server_;
    QMap<QTcpSocket*, ClientInfo> clients_;
    QMap<int, QTcpSocket*> client_sockets_;  // Reverse lookup by ID
    int next_client_id_;
    QMap<QTcpSocket*, QByteArray> receive_buffers_;  // Buffer for incomplete messages

    // Потокобезопасный доступ к настройкам
    mutable QMutex thresholds_mutex_;
    ThresholdConfig thresholds_;

    // Максимальный размер буфера приема (защита от переполнения)
    static constexpr int kMaxBufferSize = 1024 * 1024;  // 1 MB
};

#endif // TCPSERVER_H
