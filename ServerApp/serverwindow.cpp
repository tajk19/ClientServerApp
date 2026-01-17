#include "serverwindow.h"
#include "ui_serverwindow.h"

#include <QDateTime>
#include <QInputDialog>
#include <QJsonDocument>
#include <QMessageBox>

namespace {
constexpr int kMaxDataTableRows = 1000;  // Limit data table rows
}  // namespace

ServerWindow::ServerWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::ServerWindow),
      server_(new TcpServer()),
      server_thread_(new QThread(this))
{
    ui->setupUi(this);

    // Move server to separate thread
    server_->moveToThread(server_thread_);
    server_thread_->start();

    // Configure table behavior
    ui->tableClients->horizontalHeader()->setStretchLastSection(true);
    ui->tableData->horizontalHeader()->setStretchLastSection(true);

    setupConnections();
    updateButtonStates();

    appendLog("Server application started");
}

ServerWindow::~ServerWindow()
{
    // Останавливаем сервер в его потоке
    if (server_running_) {
        QMetaObject::invokeMethod(server_, "stopServer", Qt::BlockingQueuedConnection);
    }

    // Запланировать удаление сервера в его потоке
    server_->deleteLater();

    // Завершить поток
    server_thread_->quit();
    server_thread_->wait();

    delete ui;
}

void ServerWindow::setupConnections()
{
    // UI button connections
    connect(ui->btnStartServer, &QPushButton::clicked,
            this, &ServerWindow::onStartServerClicked);
    connect(ui->btnStopServer, &QPushButton::clicked,
            this, &ServerWindow::onStopServerClicked);
    connect(ui->btnStartClients, &QPushButton::clicked,
            this, &ServerWindow::onStartClientsClicked);
    connect(ui->btnStopClients, &QPushButton::clicked,
            this, &ServerWindow::onStopClientsClicked);
    connect(ui->btnSettings, &QPushButton::clicked,
            this, &ServerWindow::onSettingsClicked);

    // Server event connections (cross-thread, use queued)
    connect(server_, &TcpServer::clientConnected,
            this, &ServerWindow::onClientConnected, Qt::QueuedConnection);
    connect(server_, &TcpServer::clientDisconnected,
            this, &ServerWindow::onClientDisconnected, Qt::QueuedConnection);
    connect(server_, &TcpServer::clientStatusChanged,
            this, &ServerWindow::onClientStatusChanged, Qt::QueuedConnection);
    connect(server_, &TcpServer::dataReceived,
            this, &ServerWindow::onDataReceived, Qt::QueuedConnection);
    connect(server_, &TcpServer::logMessage,
            this, &ServerWindow::onLogMessage, Qt::QueuedConnection);
    connect(server_, &TcpServer::serverStarted,
            this, &ServerWindow::onServerStarted, Qt::QueuedConnection);
    connect(server_, &TcpServer::serverStopped,
            this, &ServerWindow::onServerStopped, Qt::QueuedConnection);
}

void ServerWindow::onStartServerClicked()
{
    // Start server on its thread
    QMetaObject::invokeMethod(server_, "startServer",
                              Qt::QueuedConnection,
                              Q_ARG(quint16, 12345));
}

void ServerWindow::onStopServerClicked()
{
    QMetaObject::invokeMethod(server_, "stopServer", Qt::QueuedConnection);
}

void ServerWindow::onStartClientsClicked()
{
    QMetaObject::invokeMethod(server_, "startAllClients", Qt::QueuedConnection);
}

void ServerWindow::onStopClientsClicked()
{
    QMetaObject::invokeMethod(server_, "stopAllClients", Qt::QueuedConnection);
}

void ServerWindow::onSettingsClicked()
{
    // Потокобезопасное получение настроек
    ThresholdConfig config = server_->getThresholds();

    bool ok;
    double latency = QInputDialog::getDouble(this, "Settings",
        "Max Latency (ms):", config.max_latency, 0, 10000, 1, &ok);
    if (!ok) return;

    double packet_loss = QInputDialog::getDouble(this, "Settings",
        "Max Packet Loss (%):", config.max_packet_loss, 0, 100, 2, &ok);
    if (!ok) return;

    int cpu = QInputDialog::getInt(this, "Settings",
        "Max CPU Usage (%):", config.max_cpu_usage, 0, 100, 1, &ok);
    if (!ok) return;

    int memory = QInputDialog::getInt(this, "Settings",
        "Max Memory Usage (%):", config.max_memory_usage, 0, 100, 1, &ok);
    if (!ok) return;

    config.max_latency = latency;
    config.max_packet_loss = packet_loss;
    config.max_cpu_usage = cpu;
    config.max_memory_usage = memory;

    // Потокобезопасное сохранение настроек
    server_->setThresholds(config);
    appendLog(QString("Settings updated: latency=%1ms, packet_loss=%2%, cpu=%3%, memory=%4%")
              .arg(latency).arg(packet_loss).arg(cpu).arg(memory));
}

void ServerWindow::onClientConnected(const ClientInfo &info)
{
    clients_[info.id] = info;
    updateClientTable();
    updateButtonStates();
}

void ServerWindow::onClientDisconnected(int client_id)
{
    clients_.remove(client_id);
    updateClientTable();
    updateButtonStates();
}

void ServerWindow::onClientStatusChanged(int client_id, bool is_running)
{
    if (clients_.contains(client_id)) {
        clients_[client_id].is_running = is_running;
        updateClientTable();
    }
}

void ServerWindow::onDataReceived(const ClientData &data)
{
    addDataToTable(data);
}

void ServerWindow::onLogMessage(const QString &message)
{
    appendLog(message);
}

void ServerWindow::onServerStarted()
{
    server_running_ = true;
    updateButtonStates();
    ui->statusbar->showMessage("Server running on port 12345");
}

void ServerWindow::onServerStopped()
{
    server_running_ = false;
    clients_.clear();
    updateClientTable();
    updateButtonStates();
    ui->statusbar->showMessage("Server stopped");
}

void ServerWindow::updateClientTable()
{
    ui->tableClients->setRowCount(0);

    for (auto it = clients_.begin(); it != clients_.end(); ++it) {
        const ClientInfo &info = it.value();
        int row = ui->tableClients->rowCount();
        ui->tableClients->insertRow(row);

        ui->tableClients->setItem(row, 0, new QTableWidgetItem(QString::number(info.id)));
        ui->tableClients->setItem(row, 1, new QTableWidgetItem(info.ip_address));
        ui->tableClients->setItem(row, 2, new QTableWidgetItem(QString::number(info.port)));

        QString status = info.is_connected ? (info.is_running ? "Running" : "Connected") : "Disconnected";
        ui->tableClients->setItem(row, 3, new QTableWidgetItem(status));
    }
}

void ServerWindow::addDataToTable(const ClientData &data)
{
    // Limit table size
    while (ui->tableData->rowCount() >= kMaxDataTableRows) {
        ui->tableData->removeRow(0);
    }

    int row = ui->tableData->rowCount();
    ui->tableData->insertRow(row);

    ui->tableData->setItem(row, 0, new QTableWidgetItem(QString::number(data.client_id)));
    ui->tableData->setItem(row, 1, new QTableWidgetItem(data.data_type));
    ui->tableData->setItem(row, 2, new QTableWidgetItem(formatDataContent(data.data_type, data.content)));
    ui->tableData->setItem(row, 3, new QTableWidgetItem(data.timestamp.toString("hh:mm:ss.zzz")));

    // Auto-scroll to bottom
    ui->tableData->scrollToBottom();
}

QString ServerWindow::formatDataContent(const QString &type, const QJsonObject &content)
{
    if (type == "NetworkMetrics") {
        return QString("bandwidth=%1, latency=%2ms, packet_loss=%3%")
            .arg(content["bandwidth"].toDouble(), 0, 'f', 2)
            .arg(content["latency"].toDouble(), 0, 'f', 2)
            .arg(content["packet_loss"].toDouble(), 0, 'f', 3);
    } else if (type == "DeviceStatus") {
        return QString("uptime=%1s, cpu=%2%, memory=%3%")
            .arg(content["uptime"].toInt())
            .arg(content["cpu_usage"].toInt())
            .arg(content["memory_usage"].toInt());
    } else if (type == "Log") {
        return QString("[%1] %2")
            .arg(content["severity"].toString())
            .arg(content["message"].toString());
    }

    // Default: return raw JSON
    return QString(QJsonDocument(content).toJson(QJsonDocument::Compact));
}

void ServerWindow::appendLog(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->textLog->append(QString("[%1] %2").arg(timestamp, message));
}

void ServerWindow::updateButtonStates()
{
    // Используем локальную копию состояния (потокобезопасно)
    bool has_clients = !clients_.isEmpty();

    ui->btnStartServer->setEnabled(!server_running_);
    ui->btnStopServer->setEnabled(server_running_);
    ui->btnStartClients->setEnabled(server_running_ && has_clients);
    ui->btnStopClients->setEnabled(server_running_ && has_clients);
}
