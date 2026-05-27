#include <QApplication>
#include "client/client.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    qRegisterMetaType<UDSMessage>("UDSMessage");
    client w;
    w.show();
    return a.exec();
}
