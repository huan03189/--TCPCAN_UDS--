#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include<QSocketNotifier>
#include <QObject>
#include"types.h"
#include"ring_buffer.h"
#ifdef Q_OS_LINUX
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#endif

class can_interface_server:public QObject{
    Q_OBJECT


public:
    static can_interface_server *instance();

    //初始化 在Linux下打开can0接口
    bool init(const QString &interfaceName);

    //发送数据
    void transmit(const CanFrameData &frame);

signals:
    void frameReceived(const CanFrameData &frame);


private slots:
    void onSocketCanActivity();
    void onSocketWriteReady();
    void onWatchdogTimeout();


private:
    can_interface_server(QObject *parent=nullptr);
    ~can_interface_server();

    static can_interface_server *m_instance;

#ifdef Q_OS_LINUX
    int m_socketFd;
    QSocketNotifier *m_notifier;
    QSocketNotifier *m_writeNotifier;
    ringbuffer m_txRingBuffer;
#endif

};

#endif // CAN_INTERFACE_H
