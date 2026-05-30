#ifndef UDS_WORKER_H
#define UDS_WORKER_H
#include <QObject>
#include <QThread>
#include <QByteArray>
#include "uds_handler.h"

class UDS_Worker : public QObject
{
    Q_OBJECT
public:
    explicit UDS_Worker(QObject *parent = nullptr);
    ~UDS_Worker();

public slots:
    void init();

    void onTcpUdsMessageReceived(const UDSMessage &msg, int clientId); // 专用于 TCP

    void onCanUdsMessageReceived(const UDSMessage &msg);//专用于CAN


private slots:
    UDSMessage processUdsRequest(const UDSMessage &msg);

private:
    UDS_handler *m_handler;

signals:

    //通信层（CanWorker/TcpWorker）需要监听这个信号
    void tcpResponseReady(const UDSMessage &msg,int clientId);

    void canResponseReady(const UDSMessage &msg);
};

#endif // UDS_WORKER_H
