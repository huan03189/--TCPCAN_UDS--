#include "uds_worker.h"
#include "server/db_logger.h"
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <QElapsedTimer>

UDS_Worker::UDS_Worker(QObject *parent)
    : QObject{parent}
    , m_handler(new UDS_handler(this))
{
}

UDS_Worker::~UDS_Worker()
{
}

void UDS_Worker::init()
{
    DB_logger::instance()->log(DB_logger::LOG_INFO,
                               "UDS Worker initialized",
                               QString("Thread: %1").arg(reinterpret_cast<quintptr>(QThread::currentThread())));
    qDebug() << "[UDS Worker] Initialized in thread:" << QThread::currentThread();
}

void UDS_Worker::onTcpUdsMessageReceived(const UDSMessage &msg, int clientId)
{
    QElapsedTimer timer;
    timer.start();

    UDSMessage response = processUdsRequest(msg);

    if (response.serviceId != 0x00) {
        qint64 latencyUs = timer.nsecsElapsed() / 1000;

        uint16_t did = 0;
        if (msg.serviceId == UDS_SID::READ_DATA_BY_IDENTIFIER && msg.payload.size() >= 2) {
            did = static_cast<uint16_t>(static_cast<uint8_t>(msg.payload.at(0))
                                        | (static_cast<uint8_t>(msg.payload.at(1)) << 8));
        }

        DB_logger::instance()->logUdsTransaction(
            "TCP", clientId,
            msg.serviceId, msg.subFunction, did, msg.payload,
            response.serviceId, response.negativeCode, response.payload,
            latencyUs);

        emit tcpResponseReady(response, clientId);
    }
}

void UDS_Worker::onCanUdsMessageReceived(const UDSMessage &msg)
{
    QElapsedTimer timer;
    timer.start();

    UDSMessage response = processUdsRequest(msg);

    if (response.serviceId != 0x00) {
        qint64 latencyUs = timer.nsecsElapsed() / 1000;

        uint16_t did = 0;
        if (msg.serviceId == UDS_SID::READ_DATA_BY_IDENTIFIER && msg.payload.size() >= 2) {
            did = static_cast<uint16_t>(static_cast<uint8_t>(msg.payload.at(0))
                                        | (static_cast<uint8_t>(msg.payload.at(1)) << 8));
        }

        DB_logger::instance()->logUdsTransaction(
            "CAN", -1,
            msg.serviceId, msg.subFunction, did, msg.payload,
            response.serviceId, response.negativeCode, response.payload,
            latencyUs);

        emit canResponseReady(response);
    }
}

UDSMessage UDS_Worker::processUdsRequest(const UDSMessage &msg)
{
    if (!m_handler) {
        qCritical() << "[UDS Worker] Handler Null";
        return UDSMessage{};
    }

    UDSMessage response;
    try {
        response = m_handler->processRequest(msg);
    } catch (...) {
        qCritical() << "[UDS Worker] Exception In processRequest";
        return UDSMessage{};
    }

    response.isResponse = true;
    response.requestId = msg.requestId;
    response.timetemp = QDateTime::currentMSecsSinceEpoch();
    return response;
}
