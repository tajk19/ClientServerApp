#include "serverwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ServerWindow s;
    s.show();
    return a.exec();
}
