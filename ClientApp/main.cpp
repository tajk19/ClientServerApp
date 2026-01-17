#include <QCoreApplication>
#include <QTextStream>

#include "client.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QTextStream out(stdout);

    // Parse command line arguments for host and port
    QString host = "localhost";
    quint16 port = 12345;

    QStringList args = a.arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args[i] == "-h" || args[i] == "--host") {
            if (i + 1 < args.size()) {
                host = args[++i];
            }
        } else if (args[i] == "-p" || args[i] == "--port") {
            if (i + 1 < args.size()) {
                port = args[++i].toUShort();
            }
        } else if (args[i] == "--help") {
            out << "Usage: ClientApp [-h|--host HOST] [-p|--port PORT]\n";
            out << "  -h, --host HOST    Server host (default: localhost)\n";
            out << "  -p, --port PORT    Server port (default: 12345)\n";
            return 0;
        }
    }

    out << "Client application starting...\n";
    out << QString("Target server: %1:%2\n").arg(host).arg(port);
    out.flush();

    Client client;

    // Connect log messages to console output
    QObject::connect(&client, &Client::logMessage, [&out](const QString &message) {
        out << "[Client] " << message << "\n";
        out.flush();
    });

    // Connect to server
    client.connectToServer(host, port);

    return a.exec();
}
