#include "can_interface.h"
#include<QDebug>
#include<QCanBus>
#include<QVariant>
#include"types.h"
#include<QThread>
#include"udshelper.h"
#include <cstdint>
#include <QApplication>

extern "C" {
#include "isotp/isotp_user.h"
#include "isotp/isotp.h"
}

Can_InterFace* Can_InterFace::m_instance = nullptr;

Can_InterFace* Can_InterFace::Instance(){
    if(!m_instance){
        m_instance = new Can_InterFace();
    }
    return m_instance;
}
void Can_InterFace::setInstance(Can_InterFace* instance){
    m_instance = instance;
}

Can_InterFace::Can_InterFace(QObject *parent):QObject(parent),m_device(nullptr),
    m_pollTimer(nullptr),m_reconnectTimer(nullptr)
{
    qDebug() << "Can_InterFace created in thread:" << QThread::currentThreadId();

    isotp_init_link(&m_iso_link, CanId::ID_UDS_REQ,
                    m_iso_send_buf, ISO_TP_BUFFER_SIZE,
                    m_iso_recv_buf, ISO_TP_BUFFER_SIZE);
    m_iso_link.receive_arbitration_id = CanId::ID_UDS_RESP;

    isotp_user_set_send_function([](uint32_t id, const uint8_t* data, uint8_t len){
        CanFrame bufFrame;
        bufFrame.id = id;
        bufFrame.len = len;
        memcpy(bufFrame.data, data, len);
        Can_InterFace* self = Can_InterFace::Instance();
        self->m_sendRingBuffer.push(bufFrame);
        self->flushSendBuffer();
    });
}


bool Can_InterFace::init(const QString &plugin,const QString &name){
    m_lastPlugin = plugin;
    m_lastIface  = name;

    // 清理旧连接
    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
        m_pollTimer = nullptr;
    }
    if (m_device) {
        m_device->disconnect();
        m_device->disconnectDevice();
        delete m_device;
        m_device = nullptr;
    }
    isotp_init_link(&m_iso_link, CanId::ID_UDS_REQ,
                    m_iso_send_buf, ISO_TP_BUFFER_SIZE,
                    m_iso_recv_buf, ISO_TP_BUFFER_SIZE);
    m_iso_link.receive_arbitration_id = CanId::ID_UDS_RESP;
    m_sendRingBuffer.reset();

    QString error;
    m_device = QCanBus::instance()->createDevice(plugin, name, &error);
    m_device->setConfigurationParameter(QCanBusDevice::BitRateKey, QVariant(500000));

    connect(m_device, &QCanBusDevice::framesReceived, this, &Can_InterFace::onFramesReceived);

    connect(m_device, &QCanBusDevice::errorOccurred, this, [this](QCanBusDevice::CanBusError err) {
        qWarning() << "CAN device error:" << err;
        if (!m_reconnectTimer || !m_reconnectTimer->isActive()) {
            if (!m_reconnectTimer) {
                m_reconnectTimer = new QTimer(this);
                m_reconnectTimer->setInterval(1000);
                connect(m_reconnectTimer, &QTimer::timeout, this, &Can_InterFace::tryReconnect);
            }
            qInfo() << "Starting auto-reconnect...";
            m_reconnectTimer->start();
        }
    });

    if (!m_device->connectDevice()) {
        qCritical() << "CAN device connect failed:" << m_device->errorString();
        return false;
    }

    QCanBusFrame dummyFrame;
    dummyFrame.setFrameId(0x7DF);
    dummyFrame.setPayload(QByteArray(1, 0x00));
    m_device->writeFrame(dummyFrame);

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, [this](){
        isotp_poll(&m_iso_link);
        uint16_t udsLen = 0;
        int ret = isotp_receive(&m_iso_link, m_iso_recv_buf, ISO_TP_BUFFER_SIZE, &udsLen);
        if (ret == ISOTP_RET_OK && udsLen > 0) {
            QByteArray raw(reinterpret_cast<char*>(m_iso_recv_buf), udsLen);
            UDSMessage msg = udshelper::deserializeUDSMessage(raw);
            emit udsMessageReceived(msg);
        }
    });
    m_pollTimer->start(100);

    if (m_reconnectTimer) m_reconnectTimer->stop();

    qDebug() << "CAN Initialized" << plugin << name;
    return true;
}

void Can_InterFace::tryReconnect()
{
    qInfo() << "Reconnect attempt..." << m_lastPlugin << m_lastIface;
    init(m_lastPlugin, m_lastIface);
}

void Can_InterFace::sendUdsMessage(const UDSMessage &msg){
    QByteArray raw = udshelper::serializeUDSMessage(msg);
    sendUdsRawData(raw);
}

void Can_InterFace::sendUdsRawData(const QByteArray &raw){
    int ret = isotp_send(&m_iso_link,
                         reinterpret_cast<const uint8_t*>(raw.constData()),
                         static_cast<uint16_t>(raw.size()));
    if(ret != ISOTP_RET_OK) qDebug() << "[ISO-TP] send error:" << ret;
    isotp_poll(&m_iso_link);
}

void Can_InterFace::transmit(const CanFrameData &frame){
    if (!m_device) return;
    CanFrame bufFrame;
    bufFrame.id = frame.id;
    bufFrame.len = frame.len;
    memcpy(bufFrame.data, frame.data, frame.len);
    if (!m_sendRingBuffer.push(bufFrame)) {
        qWarning() << "Send ring buffer full!";
        flushSendBuffer();
        m_sendRingBuffer.push(bufFrame);
    }
}

void Can_InterFace::flushSendBuffer() {
    if (!m_device) return;
    CanFrame bufFrame;
    while (m_sendRingBuffer.pop(bufFrame)) {
        QByteArray payload(reinterpret_cast<const char*>(bufFrame.data), bufFrame.len);
        QCanBusFrame qFrame;
        qFrame.setFrameId(bufFrame.id);
        qFrame.setPayload(payload);
        if (qFrame.isValid()) m_device->writeFrame(qFrame);
    }
}

void Can_InterFace::onFramesReceived(){
    if (!m_device) return;
    while(m_device && m_device->framesAvailable()){
        QCanBusFrame qFrame = m_device->readFrame();
        if(!qFrame.isValid()) continue;
        emit frameReceivedSignal(qFrame);
        if (qFrame.frameId() != m_iso_link.receive_arbitration_id) continue;
        QByteArray payload = qFrame.payload();
        const uint8_t* data = reinterpret_cast<const uint8_t*>(payload.constData());
        size_t len = static_cast<size_t>(payload.size());
        isotp_on_can_message(&m_iso_link, const_cast<uint8_t*>(data), len);
        isotp_poll(&m_iso_link);
        uint16_t udsLen = 0;
        int ret = isotp_receive(&m_iso_link, m_iso_recv_buf, ISO_TP_BUFFER_SIZE, &udsLen);
        if(ret == ISOTP_RET_OK && udsLen > 0){
            QByteArray raw(reinterpret_cast<char*>(m_iso_recv_buf), udsLen);
            UDSMessage msg = udshelper::deserializeUDSMessage(raw);
            emit udsMessageReceived(msg);
        }
    }
}
