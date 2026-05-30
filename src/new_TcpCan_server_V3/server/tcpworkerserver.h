#ifndef TCPWORKERSERVER_H
#define TCPWORKERSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include "core/protocol_parser.h"
#include "core/types.h"

// TcpWorkerServer
//
// 职责：
//
// 1. TCP客户端管理
// 2. TCP数据收发
// 3. parser管理
// 4. TCP协议层 <-> UDS业务层 转换
// 5. 广播CAN数据
//
// 注意：
// 不处理UDS业务逻辑
class TcpWorkerServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpWorkerServer(QObject *parent = nullptr);
    ~TcpWorkerServer();


public slots:
    //初始化服务器套接字
    void initServer();


    //启动服务器监听
    bool startServer(quint16 port);


    //停止服务器
    void stopServer();


    void sendTcpResponse(const UDSMessage &msg, int clientId);


signals:
    //新客户端连接
    void clientConnected(QTcpSocket* client);

    //客户端断开连接
    void clientDisconnected(QTcpSocket* client);

    //收到UDS消息
    void udsMessageReceived(const UDSMessage &msg,int clientId);

    //服务器状态变化
    void serverStarted(quint16 port);
    void serverStopped();


private slots:
    //新客户端连接处理
    void onNewConnection();

    //客户端数据到达处理
    void onClientReadyRead();

    //客户端断开连接处理
    void onClientDisconnected();

private:
    //TCP Server
    QTcpServer* m_server;
    //解析器
    QHash<QTcpSocket*,protocol_parser*>m_clientParsers;
    //客户端列表
    QList<QTcpSocket*>m_clients;
    QMap<int, QTcpSocket*> m_clientMap;
    int m_nextClientId = 1001;

    //为新客户端初始化相关资源
    void setupClientResources(QTcpSocket*client);


    //清理客户端相关资源
    void cleanupClientResources(QTcpSocket*client);


    QByteArray constructUdsPacket(const UDSMessage &msg);

};

#endif // TCPWORKERSERVER_H
