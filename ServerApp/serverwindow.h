#ifndef SERVERWINDOW_H
#define SERVERWINDOW_H

#include <QMainWindow>
#include <QThread>

#include "tcpserver.h"

namespace Ui {
class ServerWindow;
}

class ServerWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ServerWindow(QWidget *parent = nullptr);
    ~ServerWindow();

private slots:
    // UI button handlers
    void onStartServerClicked();
    void onStopServerClicked();
    void onStartClientsClicked();
    void onStopClientsClicked();
    void onSettingsClicked();

    // Server event handlers
    void onClientConnected(const ClientInfo &info);
    void onClientDisconnected(int client_id);
    void onClientStatusChanged(int client_id, bool is_running);
    void onDataReceived(const ClientData &data);
    void onLogMessage(const QString &message);
    void onServerStarted();
    void onServerStopped();

private:
    void setupConnections();
    void updateClientTable();
    void addDataToTable(const ClientData &data);
    void appendLog(const QString &message);
    void updateButtonStates();

    // Format JSON content for display based on data type
    QString formatDataContent(const QString &type, const QJsonObject &content);

    Ui::ServerWindow *ui;
    TcpServer *server_;
    QThread *server_thread_;

    // Локальная копия состояния сервера (для потокобезопасности)
    bool server_running_ = false;

    // Client data for table display
    QMap<int, ClientInfo> clients_;
};

#endif // SERVERWINDOW_H
