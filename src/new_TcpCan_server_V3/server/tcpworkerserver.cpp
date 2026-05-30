#include "tcpworkerserver.h"
#include <QDebug>
#include <QThread>
#include "core/CRC_helper.h"
#include "db_logger.h"
#include "core/udshelper.h"

TcpWorkerServer::TcpWorkerServer(QObject *parent)
    : QObject{parent}
    ,m_server(nullptr)
{
}


TcpWorkerServer::~TcpWorkerServer(){
    stopServer();

    //清理所有剩余的客户端资源
    for(QTcpSocket*client:m_clients){
        if(client){
            client->disconnectFromHost();
            client->deleteLater();
        }
    }

    m_clients.clear();

    // 清理解析器
    for (auto parser : m_clientParsers) {
        delete parser;
    }

    m_clientParsers.clear();

    if (m_server) {
        m_server->deleteLater();
    }

}


void TcpWorkerServer::initServer(){
    if(m_server){
        return;
    }

    m_server=new QTcpServer();

    connect(m_server,&QTcpServer::newConnection,this,&TcpWorkerServer::onNewConnection,Qt::QueuedConnection);
}


bool TcpWorkerServer::startServer(quint16 port){
    if(!m_server){
        qCritical() << "TCP Server not initialized";
        return false;
    }
    if (m_server->isListening()) {
        qWarning() << "Server is already listening on port" << m_server->serverPort();
        return false;
    }

    bool result = m_server->listen(QHostAddress::Any, port);
    if (result) {
        qDebug() << "TCP Server started on port" << port;
        DB_logger::instance()->log(DB_logger::LOG_INFO,
                                   "TCP Server started",
                                   QString("Port: %1").arg(port));
        emit serverStarted(port);
    } else {
        qCritical() << "Failed to start TCP Server on port" << port << ":" << m_server->errorString();
        DB_logger::instance()->log(DB_logger::LOG_ERROR,
                                   "TCP Server start failed",
                                   QString("Port: %1  Error: %2").arg(port).arg(m_server->errorString()));
    }

    return result;
}

void TcpWorkerServer::stopServer(){
    if(m_server && m_server->isListening())
    {
        m_server->close();
    }

    //断开所有客户端连接
    for(QTcpSocket*client:m_clients){
        if(client &&
            client->state() == QAbstractSocket::ConnectedState)
        {
            client->disconnectFromHost();
        }
    }
    qDebug()<<"TCP Server stopped";
    DB_logger::instance()->log(DB_logger::LOG_INFO, "TCP Server stopped");
    emit serverStopped();
}

void TcpWorkerServer::onNewConnection(){
    QTcpSocket* client=m_server->nextPendingConnection();
    if(!client){
        return;
    }

    int clientId = m_nextClientId++;
    client->setProperty("clientId", clientId);

    m_clientMap.insert(clientId, client);


    connect(client, &QTcpSocket::disconnected, this, [this, client, clientId](){
        m_clientMap.remove(clientId);
        client->deleteLater();
    });

    setupClientResources(client);
    m_clients.append(client);

    QString clientInfo = QString("%1:%2")
                             .arg(client->peerAddress().toString())
                             .arg(client->peerPort());
    DB_logger::instance()->logClientEvent("Client Connected",clientInfo);
}

void TcpWorkerServer::onClientDisconnected(){

    QTcpSocket*client=qobject_cast<QTcpSocket*>(sender());
    if(!client){
        return;
    }
    QString clientInfo = QString("%1:%2")
                             .arg(client->peerAddress().toString())
                             .arg(client->peerPort());
    // 记录客户端断开事件
    DB_logger::instance()->logClientEvent("Client Disconnected", clientInfo);

    // 从客户端列表移除
    m_clients.removeAll(client);

    // 清理该客户端的相关资源
    cleanupClientResources(client);
    emit clientDisconnected(client);

    // 删除客户端socket
    client->deleteLater();
}

void TcpWorkerServer::onClientReadyRead(){
    QTcpSocket* client=qobject_cast<QTcpSocket*>(sender());
    if(!client){
        return;
    }

    //获取对应解析器
    protocol_parser* parser=m_clientParsers.value(client);

    if(!parser){
        qWarning()<<"No parser found for client";
        return;
    }
    //读取数据并传输给解析器
    QByteArray data=client->readAll();
    parser->pushData(data);
}


void TcpWorkerServer::setupClientResources(QTcpSocket* client){
    //创建解析器
    protocol_parser* parser=new protocol_parser();

    m_clientParsers[client]=parser;

    int clientId = client->property("clientId").toInt();

    connect(parser, &protocol_parser::udsPacketReady, this, [this, clientId](const QByteArray &udsPacket){
        UDSMessage msg = udshelper::deserializeUDSMessage(udsPacket);
        qDebug() << "[Parser] Deserialized UDSMessage ->"
                 << "serviceId:" << QString::number(msg.serviceId,16)
                 << "subFunction:" << QString::number(msg.subFunction,16)
                 << "payload:" << msg.payload.toHex()
                 << "isResponse:" << msg.isResponse;
        emit udsMessageReceived(msg, clientId);
    });
    //连接客户端信号
    connect(client,&QTcpSocket::readyRead,this,&TcpWorkerServer::onClientReadyRead);
    connect(client,&QTcpSocket::disconnected,this,&TcpWorkerServer::onClientDisconnected);
}

void TcpWorkerServer::cleanupClientResources(QTcpSocket *client){
    // 移除解析器
    auto parserIt = m_clientParsers.find(client);
    if (parserIt != m_clientParsers.end()) {
        delete parserIt.value();
        m_clientParsers.erase(parserIt);
    }

    m_clients.removeAll(client);
}

void TcpWorkerServer::sendTcpResponse(const UDSMessage&msg, int clientId){

    QTcpSocket *client = m_clientMap.value(clientId, nullptr);

    if (!client) {
        return;
    }

    if (client->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    QByteArray packet = constructUdsPacket(msg);

    client->write(packet);

    client->flush();

}

QByteArray TcpWorkerServer::constructUdsPacket(
    const UDSMessage &msg)
{
    QByteArray payload =
        udshelper::serializeUDSMessage(msg);

    QByteArray header;

    header.append(char(0xCD));
    header.append(char(0xAB));

    header.append(char(3));

    uint16_t len =
        payload.size();

    header.append(char(len & 0xFF));
    header.append(char((len >> 8) & 0xFF));

    uint16_t crc =
        CRC_helper::calculate(payload);

    QByteArray packet;

    packet.append(header);

    packet.append(payload);

    packet.append(char(crc & 0xFF));
    packet.append(char((crc >> 8) & 0xFF));

    return packet;
}
