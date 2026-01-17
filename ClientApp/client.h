#ifndef CLIENT_H
#define CLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonObject>

// Client states
enum class ClientState {
    Disconnected,
    Connecting,
    WaitingConfirmation,
    WaitingStart,
    Running,
    Stopped
};

class Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(QObject *parent = nullptr);
    ~Client();

    // Connection control
    void connectToServer(const QString &host = "localhost", quint16 port = 12345);
    void disconnect();

    // State
    ClientState state() const;
    int clientId() const;

signals:
    void stateChanged(ClientState state);
    void logMessage(const QString &message);
    void connected();
    void disconnected();

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError error);
    void onReconnectTimer();
    void onSendDataTimer();

private:
    // Send JSON message to server
    void sendMessage(const QJsonObject &message);

    // Process received message from server
    void processServerMessage(const QByteArray &data);

    // Generate random data
    QJsonObject generateNetworkMetrics();
    QJsonObject generateDeviceStatus();
    QJsonObject generateLogMessage();

    // Generate random string of specified length
    QString generateRandomString(int min_length, int max_length);

    void setState(ClientState state);
    void scheduleNextSend();

    QTcpSocket *socket_;
    QTimer *reconnect_timer_;
    QTimer *send_timer_;
    QByteArray receive_buffer_;

    QString host_;
    quint16 port_;
    int client_id_;
    ClientState state_;

    // Simulated device state
    int uptime_;
    int message_counter_;
};

#endif // CLIENT_H
