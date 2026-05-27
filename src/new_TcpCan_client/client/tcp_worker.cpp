#include "tcp_worker.h"
#include<QDebug>
#include<QTimer>
#include"core/CRC_helper.h"
#include<QThread>
#include<QtEndian>
#include"core/udshelper.h"
Tcp_worker::Tcp_worker(QObject *parent)
    : QObject{parent},m_socket(nullptr)
{
    m_reconnectTimer = new QTimer(this);
    m_heartbeatTimer = new QTimer(this);
    //解析器
    connect(&m_parser, &protocol_parser::parseError, this, [](QString msg){
        qDebug() << "[Parser Error]" << msg;
    });

    connect(&m_parser,&protocol_parser::udsMessageReady,this,&Tcp_worker::udsMessageReceived);

    //配置重连定时器
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer,&QTimer::timeout,this,&Tcp_worker::onReconnectTimer);//定时器超时，触发重连逻辑

    //配置心跳定时器
    m_heartbeatTimer->setInterval(2000);

    //发送心跳逻辑
    connect(m_heartbeatTimer,&QTimer::timeout,this,[this](){
        if(m_socket->state()==QAbstractSocket::ConnectedState){
            sendHeartbeat();
        }
    });
    //m_reconnectTimer->start();
}
void Tcp_worker::initSocket()
{
    if (m_socket) {
        qDebug() << "[TCP Worker] Socket already exists, skipping initialization";
        return;
    }
    m_socket = new QTcpSocket(this);

    connect(m_socket, &QTcpSocket::connected, this, &Tcp_worker::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &Tcp_worker::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &Tcp_worker::onReadyRead);

    qDebug() << "[TCP Worker] Socket initialized in thread:" << QThread::currentThread();
}
Tcp_worker::~Tcp_worker(){
    if (m_socket) {
        m_socket->close();
        m_socket->deleteLater();
    }
}

void Tcp_worker::connectToServer(const QString &ip,quint16 port){
    if (!m_socket) {
        initSocket();
    }

    // 检查当前连接状态，避免重复连接
    if (m_socket->state() == QAbstractSocket::ConnectingState ||
        m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "Already connecting or connected to server. Current state:" << m_socket->state();
        return;
    }

    m_serverIp=ip;
    m_serverPort=port;
    qDebug() << "Attempting to connect to" << ip << ":" << port;
    m_socket->connectToHost(ip,port);
}

void Tcp_worker::DisconnectToServer(){
    if (!m_socket) {
        return;
    }
    isUserDisconnect=true;
     m_reconnectTimer->stop();
    // 检查当前连接状态，避免重复连接
    if (m_socket->state() == QAbstractSocket::ConnectingState ||
        m_socket->state() == QAbstractSocket::ConnectedState) {
        m_socket->disconnectFromHost();
        return;
    }
}


void Tcp_worker::onReconnectTimer(){
    if (!m_socket || isUserDisconnect) return;
    if (m_socket->state() == QAbstractSocket::UnconnectedState) {
        emit reconnect();
        m_socket->connectToHost(m_serverIp, m_serverPort);
    }
}
void Tcp_worker::onConnected(){
    qDebug()<<"Connected!";
    m_reconnectTimer->stop();
    m_heartbeatTimer->start();
    emit connected();
}
void Tcp_worker::onDisconnected(){
    qDebug()<<"Disconnected!";
    m_heartbeatTimer->stop();
    if(isUserDisconnect==false){
        m_reconnectTimer->start();
    }
    emit disconnected();
}
void Tcp_worker::onReadyRead(){
    //读取所有数据并给解析器
    m_parser.pushData(m_socket->readAll());
}
void Tcp_worker::sendUdsData(const QByteArray&data){
    if(!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    sendPacket(3,data);
}

void Tcp_worker::sendUdsMessage(const UDSMessage&msg)
{

    if(!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;

    QByteArray raw =
        udshelper::serializeUDSMessage(msg);

    qDebug() << "[TCP UDS SEND]"
             << raw.toHex();

    sendPacket(3, raw);
}

void Tcp_worker::sendPacket(uint8_t type, const QByteArray &payload)
{

    // 构造头部: Magic(2) + Type(1) + Length(2)
    QByteArray headerBlock;
    headerBlock.append(static_cast<char>(0xCD)); // Magic 0xABCD 小端序
    headerBlock.append(static_cast<char>(0xAB));
    headerBlock.append(static_cast<char>(type));



    uint8_t lenLow = static_cast<char>(payload.size() & 0xFF);
    uint8_t lenHigh = static_cast<char>((payload.size() >> 8) & 0xFF);
    headerBlock.append(static_cast<char>(lenLow));
    headerBlock.append(static_cast<char>(lenHigh));


    // CRC 校验 (只计算 Payload 的 CRC)
    uint16_t crc = CRC_helper::calculate(payload);


    // 组装完整包: Header + Payload + CRC(2)
    QByteArray packet;
    packet.append(headerBlock);
    packet.append(payload);
    uint8_t crcLow = static_cast<char>(crc & 0xFF);
    uint8_t crcHigh = static_cast<char>((crc >> 8) & 0xFF);
    packet.append(static_cast<char>(crcLow));       // CRC Low
    packet.append(static_cast<char>(crcHigh));      // CRC High
    qint64 bytesWritten = m_socket->write(packet);
    if (bytesWritten != packet.size()) {
        qDebug() << "[CLIENT PKT] WARNING: Only wrote" << bytesWritten << "out of" << packet.size() << "bytes!";
    } else {
        qDebug() << "[CLIENT PKT] Successfully sent full packet";
    }

    // 强制刷新确保数据立即发送
    m_socket->flush();
}


void Tcp_worker::sendHeartbeat(){
    if(!m_socket || m_socket->state() != QAbstractSocket::ConnectedState) return;
    sendPacket(2, QByteArray());
}

