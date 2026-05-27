#include "can_worker.h"
#include <QDebug>
#include <core/udshelper.h>
#include <QThread>

Can_worker::Can_worker(QObject *parent)
    : QObject{parent}, m_interface(nullptr)
{
}

bool Can_worker::init(const QString &plugin, const QString &iface)
{
    if (!Can_InterFace::Instance()) {
        Can_InterFace* can = new Can_InterFace();
        Can_InterFace::setInstance(can);
    }

    m_interface = Can_InterFace::Instance();
    m_interface->moveToThread(QThread::currentThread());

    disconnect(m_interface, &Can_InterFace::udsMessageReceived,
               this, &Can_worker::onUdsMessageReceived);
    disconnect(m_interface, &Can_InterFace::frameSentSignal,
               this, &Can_worker::signalFrameSent);
    disconnect(m_interface, &Can_InterFace::frameReceivedSignal,
               this, &Can_worker::signalFrameReceived);

    if (!m_interface->init(plugin, iface)) {
        qDebug() << "[Can_worker] CAN init failed!";
        return false;
    }

    connect(m_interface, &Can_InterFace::udsMessageReceived,
            this, &Can_worker::onUdsMessageReceived, Qt::DirectConnection);
    connect(m_interface, &Can_InterFace::frameSentSignal,
            this, &Can_worker::signalFrameSent, Qt::DirectConnection);
    connect(m_interface, &Can_InterFace::frameReceivedSignal,
            this, &Can_worker::signalFrameReceived, Qt::DirectConnection);

    qDebug() << "[Can_worker] CAN initialized successfully";
    return true;
}

void Can_worker::sendUdsData(const QByteArray &data)
{
    m_interface->sendUdsRawData(data);
}

void Can_worker::sendUdsMessage(const UDSMessage &msg)
{
    emit udsRequestSentSignal(msg);
    m_interface->sendUdsMessage(msg);
}

void Can_worker::onUdsMessageReceived(const UDSMessage &msg)
{
    if (msg.serviceId == 0) return;
    emit udsResponseReceivedSignal(msg);
    emit udsMessageReceived(msg);
}
