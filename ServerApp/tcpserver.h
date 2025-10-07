#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>

class TcpServer : QObject
{
    Q_OBJECT
public:
    TcpServer();
    ~TcpServer();
};

#endif // TCPSERVER_H
