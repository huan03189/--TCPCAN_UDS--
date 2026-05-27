#ifndef CAN_WORKER_SERVER_H
#define CAN_WORKER_SERVER_H

#include <QObject>
#include "core/can_interface.h"
#include "core/types.h"
#include "isotp/isotp.h"
#include <QTimer>
#include <QByteArray>

struct IsoTpLink;

class Can_worker_server : public QObject
{
    Q_OBJECT
public:
    explicit Can_worker_server(QObject *parent = nullptr);
    ~Can_worker_server();
    //初始化CAN接口
    bool init(const QString &interfacename="can0");
public slots:
    // New method to send UDS response via CAN/ISO-TP
    void sendUdsMessage(const UDSMessage &msg);
signals:
    //接收到CAN帧时发出的信号
    void frameReceived(const CanFrameData &frame);

    //传递解析后的完整的UDS消息
    void udsMessageReceived(const UDSMessage &udsMessage);

private slots:
    //处理从CAN接口接收到的帧
    void onFrameReceived(const CanFrameData &frame);
private:
    can_interface_server *m_interface;
    bool m_initialized;
    // ISO-TP Link instance
    IsoTpLink* m_isoTpLink;
    // Buffers for ISO-TP
    QByteArray m_isoTpSendBuffer;
    QByteArray m_isoTpReceiveBuffer;
    // Timer for polling ISO-TP
    QTimer* m_pollTimer;
    void startIsoTpPolling();
    void stopIsoTpPolling();
};

#endif // CAN_WORKER_SERVER_H
