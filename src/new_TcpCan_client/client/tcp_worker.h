#ifndef TCP_WORKER_H
#define TCP_WORKER_H

#include <QObject>
#include<QTcpSocket>
#include"core/protocol_parser.h"
#include"core/types.h"


//处理Tcp连接、粘包处理和重连逻辑
class Tcp_worker : public QObject
{
    Q_OBJECT
public:
    explicit Tcp_worker(QObject *parent = nullptr);
    ~Tcp_worker();
public slots:
    void initSocket();

    //供外部调用接口
    void connectToServer(const QString &ip,quint16 port);//连接至服务器
    void sendUdsMessage(const UDSMessage&msg);
    void sendUdsData(const QByteArray& msg);
    void sendHeartbeat();//发送心跳
    void DisconnectToServer();
signals:
    void connected();
    void disconnected();
    void reconnect();
    void udsMessageReceived(const UDSMessage&msg);//uds数据接收信号

    //内部核心槽函数
private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onReconnectTimer();

private:

    QTcpSocket *m_socket;
    protocol_parser m_parser;//解析器
    QTimer *m_reconnectTimer;
    QTimer *m_heartbeatTimer;
    QString m_serverIp;
    quint16 m_serverPort;
    bool isUserDisconnect=false;
    //通用打包函数
    void sendPacket(uint8_t type, const QByteArray &payload);
};

#endif // TCP_WORKER_H
