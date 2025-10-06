#include "clientwindow.h"
#include "serverwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    ClientWindow c;
    ServerWindow s;
    c.show();
    return a.exec();
}
