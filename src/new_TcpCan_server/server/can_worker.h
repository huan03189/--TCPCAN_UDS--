#ifndef CAN_WORKER_H
#define CAN_WORKER_H

#include <QObject>
#include "can_worker_server.h"
#include "core/types.h"
class CanServer : public QObject
{
    Q_OBJECT
public:
    explicit CanServer(Can_worker_server* worker,QObject *parent = nullptr);

    ~CanServer();

signals:
    void udsMessageReceived(const UDSMessage &msg);
private:
    Can_worker_server *m_canworker = nullptr;
public slots:
    //处理从CAN Worker传来的帧
    void onUdsMessageReceived(const UDSMessage &msg);

    //发CAN响应
    void sendCanResponse(const UDSMessage &msg);
};

#endif // CAN_WORKER_H
