#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include<QObject>
#include<QCanBusDevice>
#include<QTimer>
#include"ring_buffer.h"
#include"types.h"
#include"isotp/isotp.h"

#define ISO_TP_BUFFER_SIZE 1024

class Can_InterFace : public QObject
{
    Q_OBJECT
public:
    static Can_InterFace* Instance();
    explicit Can_InterFace(QObject*parent=nullptr);

    bool init(const QString &plugin, const QString &name);
    static void setInstance(Can_InterFace* instance);

    void transmit(const CanFrameData &frame);
    void sendUdsMessage(const UDSMessage &msg);
    void sendUdsRawData(const QByteArray &raw);

signals:
    void udsMessageReceived(const UDSMessage &msg);
    void frameSentSignal(const QCanBusFrame &frame);
    void frameReceivedSignal(const QCanBusFrame &frame);

private slots:
    void onFramesReceived();
    void tryReconnect();

private:
    static Can_InterFace *m_instance;
    QCanBusDevice *m_device;

    IsoTpLink m_iso_link;
    uint8_t m_iso_send_buf[ISO_TP_BUFFER_SIZE];
    uint8_t m_iso_recv_buf[ISO_TP_BUFFER_SIZE];
    QTimer* m_pollTimer;

    QTimer* m_reconnectTimer;
    QString m_lastPlugin;
    QString m_lastIface;

    ringbuffer m_sendRingBuffer;
    void flushSendBuffer();
};

#endif // CAN_INTERFACE_H
