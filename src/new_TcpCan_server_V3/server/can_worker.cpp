#include "can_worker.h"
#include <QDebug>
#include "db_logger.h"
CanServer::CanServer(Can_worker_server *worker,QObject *parent)
    : QObject{parent},m_canworker(worker)
{
    connect(m_canworker,&Can_worker_server::udsMessageReceived,this,&CanServer::onUdsMessageReceived);

    DB_logger::instance()->log(DB_logger::LOG_INFO,
                               "CanServer initialized",
                               "Ready to forward CAN frames to UDS Layer");
}

CanServer::~CanServer(){
}

void CanServer::onUdsMessageReceived(const UDSMessage &msg){
    emit udsMessageReceived(msg);
}


void CanServer::sendCanResponse(const UDSMessage &msg){

    if(!m_canworker)
    {
        qCritical()
        << "[CanServer] Worker null";

        return;
    }
    m_canworker->sendUdsMessage(msg);
}
