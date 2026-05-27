#ifndef CAN_WORKER_H
#define CAN_WORKER_H

#include <QObject>
#include "core/types.h"
#include "core/can_interface.h"

class Can_worker : public QObject
{
    Q_OBJECT
public:
    explicit Can_worker(QObject *parent = nullptr);

public slots:
    bool init(const QString &plugin, const QString &iface);
    void sendUdsMessage(const UDSMessage&msg);
    void sendUdsData(const QByteArray&data);
    void onUdsMessageReceived(const UDSMessage&msg);

signals:
    void udsMessageReceived(const UDSMessage&msg);
    void udsRequestSentSignal(const UDSMessage&requestData);
    void udsResponseReceivedSignal(const UDSMessage&responseData);
    void signalFrameSent(const QCanBusFrame &frame);
    void signalFrameReceived(const QCanBusFrame &frame);

private:
    Can_InterFace* m_interface;
};

#endif // CAN_WORKER_H
