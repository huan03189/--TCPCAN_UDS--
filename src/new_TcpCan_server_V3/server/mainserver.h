#ifndef MAINSERVER_H
#define MAINSERVER_H

#include <QObject>
#include <QThread>
#include "can_worker.h"
#include "tcpworkerserver.h"
#include "core/uds_worker.h"

class MainServer : public QObject
{
    Q_OBJECT
public:
    explicit MainServer(QObject *parent = nullptr);
    ~MainServer();


    //启动服务器
    bool start(quint16 tcpPort=8888,const QString &caninterface="can0");

    //停止服务器
    void stop();

private:
    // CAN Thread
    QThread* m_canThread = nullptr;

    Can_worker_server* m_canWorker = nullptr;

    CanServer* m_canServer = nullptr;

    // TCP Thread
    QThread* m_tcpThread = nullptr;

    TcpWorkerServer* m_tcpWorkerServer = nullptr;

    // UDS Thread
    QThread* m_udsThread = nullptr;

    UDS_Worker* m_udsWorker = nullptr;

    //线程初始化
    void setupCanThread(const QString &caninterface);
    void setupTcpThread(quint16 tcpPort);
    //Uds线程
    void setupUdsThread();


private slots:
    //Tcp客户端连接处理
    void onClientConnected(QTcpSocket* client);

    //Tcp客户端断开处理
    void onClientDisconnected(QTcpSocket* client);

signals:
    void serverStarted();
    void serverStopped();
};

#endif // MAINSERVER_H
