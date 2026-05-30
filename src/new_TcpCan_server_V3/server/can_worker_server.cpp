#include "can_worker_server.h"
#include "db_logger.h"
#include <QDebug>
#include "isotp/isotp_user.h"
#include "core/udshelper.h"
#include <QThread>
Can_worker_server::Can_worker_server(QObject *parent)
    : QObject{parent},
    m_interface(nullptr),
    m_initialized(false),
    m_isoTpLink(nullptr),
    m_pollTimer(nullptr)
{
}

Can_worker_server::~Can_worker_server(){

    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }

    delete m_isoTpLink;
    m_isoTpLink = nullptr;

    qDebug() << "CanWorkerServer destroyed";
}

bool Can_worker_server::init(const QString &interfacename){

    m_interface = can_interface_server::instance();
    m_interface->moveToThread(QThread::currentThread());

    qDebug() << "[Can_worker_server] can_interface_server moved to thread:"
             << m_interface->thread();
    // CAN接收（同线程，用直接连接避免事件队列开销）
    connect(m_interface, &can_interface_server::frameReceived,
            this, &Can_worker_server::onFrameReceived,
            Qt::DirectConnection);
    qDebug() << "[Can_worker_server] Connected to frameReceived. m_interface:" << m_interface
             << "this:" << this << "thread:" << QThread::currentThread();
    // ISO-TP buffer
    const int bufferSize = 4096;
    m_isoTpSendBuffer.resize(bufferSize);
    m_isoTpReceiveBuffer.resize(bufferSize);

    m_isoTpLink = new IsoTpLink();

    isotp_init_link(m_isoTpLink,
                    CanId::ID_UDS_RESP,
                    reinterpret_cast<uint8_t*>(m_isoTpSendBuffer.data()),
                    static_cast<uint16_t>(m_isoTpSendBuffer.size()),
                    reinterpret_cast<uint8_t*>(m_isoTpReceiveBuffer.data()),
                    static_cast<uint16_t>(m_isoTpReceiveBuffer.size()));

    // 发送函数
    isotp_user_set_send_function([](uint32_t arbitration_id, const uint8_t* data, uint8_t len) {
        CanFrameData frame;
        frame.id = arbitration_id;
        frame.len = qMin(len, (uint8_t)8);
        memcpy(frame.data, data, frame.len);

        can_interface_server::instance()->transmit(frame);
    });

    // 初始化 CAN
    bool result = m_interface->init(interfacename);

    if(result){
        m_initialized = true;
        qDebug()<<"CanWorkerServer initialized successfully on"<<interfacename;
        DB_logger::instance()->log(DB_logger::LOG_INFO,
                                   "CAN Worker initialized",
                                   QString("Interface: %1").arg(interfacename));
    } else {
        DB_logger::instance()->log(DB_logger::LOG_ERROR,
                                   "CAN Worker init failed",
                                   QString("Interface: %1").arg(interfacename));
    }

    m_pollTimer = new QTimer(this);

    connect(m_pollTimer, &QTimer::timeout, this, [this]() {
        if (m_isoTpLink && m_initialized) {
            isotp_poll(m_isoTpLink);
            if (m_pollTimer->interval() != 100) {
                m_pollTimer->setInterval(100);
            }
        }
    });

    m_pollTimer->start(100);

    return result;
}

void Can_worker_server::onFrameReceived(const CanFrameData &frame){
    emit frameReceived(frame);

    if (!m_isoTpLink)
        return;

    // 只处理 UDS 请求帧
    if (frame.id != CanId::ID_UDS_REQ)
        return;

    // 喂给 ISO-TP
    isotp_on_can_message(
        m_isoTpLink,
        frame.data,
        frame.len);


    // 尝试取完整UDS
    uint16_t receivedSize = 0;

    int ret = isotp_receive(
        m_isoTpLink,
        reinterpret_cast<uint8_t*>(m_isoTpReceiveBuffer.data()),
        static_cast<uint16_t>(m_isoTpReceiveBuffer.size()),
        &receivedSize);

    if(ret == ISOTP_RET_OK &&
        receivedSize > 0)
    {
        QByteArray udsRaw(
            reinterpret_cast<const char*>(m_isoTpReceiveBuffer.constData()),
            receivedSize);

        UDSMessage msg = udshelper::deserializeUDSMessage(udsRaw);
        msg.isResponse = true;

        emit udsMessageReceived(msg);
    }
}

void Can_worker_server::sendUdsMessage(const UDSMessage &msg)
{
    if (!m_initialized || !m_isoTpLink)
    {
        qDebug() << "ISO-TP not ready";

        return;
    }

    QByteArray udsRaw =udshelper::serializeUDSMessage(msg);

    int result =
        isotp_send(
            m_isoTpLink,
            reinterpret_cast<const uint8_t*>(
                udsRaw.constData()),
            static_cast<uint16_t>(
                udsRaw.size()));


    if(result != ISOTP_RET_OK &&
        result != ISOTP_RET_INPROGRESS)
    {
        qDebug() << "ISO-TP send failed:" << result;
        DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                   "ISO-TP send failed",
                                   QString("Result: %1").arg(result));
    }
}
