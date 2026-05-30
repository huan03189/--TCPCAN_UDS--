#include "mainserver.h"
#include "db_logger.h"
#include <QDebug>
#include <QMetaObject>

#ifdef Q_OS_LINUX
#include <pthread.h>

static void bindThreadToCore(int core, const char *name) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
    if (ret != 0) {
        qWarning() << "Failed to bind" << name << "to core" << core << ":" << strerror(ret);
        DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                   QString("Thread binding failed: %1").arg(name),
                                   QString("Core %1: %2").arg(core).arg(strerror(ret)));
    } else {
        qInfo() << "Thread" << name << "bound to CPU core" << core;
        DB_logger::instance()->log(DB_logger::LOG_INFO,
                                   QString("Thread bound: %1 → Core %2").arg(name).arg(core));
    }
}
#endif

MainServer::MainServer(QObject *parent)
    : QObject{parent}
    , m_canThread(nullptr)
    , m_canWorker(nullptr)
    , m_canServer(nullptr)
    , m_tcpThread(nullptr)
    , m_tcpWorkerServer(nullptr)
    , m_udsThread(nullptr)
    , m_udsWorker(nullptr)
{
}

MainServer::~MainServer()
{
    stop();
}

bool MainServer::start(quint16 tcpPort, const QString &caninterface)
{
    DB_logger::instance()->log(DB_logger::LOG_INFO,
                               "MainServer starting",
                               QString("Port: %1  CAN: %2").arg(tcpPort).arg(caninterface));
    qDebug() << "Starting Main Server...";

    setupUdsThread();
    setupCanThread(caninterface);
    setupTcpThread(tcpPort);

    emit serverStarted();

    DB_logger::instance()->log(DB_logger::LOG_INFO, "MainServer started successfully");
    qDebug() << "Main Server started successfully";

    return true;
}

void MainServer::stop()
{
    DB_logger::instance()->log(DB_logger::LOG_INFO, "MainServer stopping");
    qDebug() << "[MainServer] Stopping...";

    if (m_udsWorker) {
        m_udsWorker->disconnect();
    }

    // CAN Thread
    if (m_canThread) {
        if (m_canWorker) {
            m_canWorker->deleteLater();
            m_canWorker = nullptr;
        }
        if (m_canServer) {
            m_canServer->deleteLater();
            m_canServer = nullptr;
        }
        m_canThread->quit();
        m_canThread->wait();
        delete m_canThread;
        m_canThread = nullptr;
    }

    // TCP Thread
    if (m_tcpThread) {
        if (m_tcpWorkerServer) {
            QMetaObject::invokeMethod(m_tcpWorkerServer, "stopServer", Qt::BlockingQueuedConnection);
            m_tcpWorkerServer->deleteLater();
            m_tcpWorkerServer = nullptr;
        }
        m_tcpThread->quit();
        m_tcpThread->wait();
        delete m_tcpThread;
        m_tcpThread = nullptr;
    }

    // UDS Thread
    if (m_udsThread) {
        m_udsThread->quit();
        m_udsThread->wait();
    }
    if (m_udsWorker) {
        m_udsWorker->deleteLater();
        m_udsWorker = nullptr;
    }
    if (m_udsThread) {
        delete m_udsThread;
        m_udsThread = nullptr;
    }

    emit serverStopped();
    DB_logger::instance()->log(DB_logger::LOG_INFO, "MainServer stopped");
    qDebug() << "[MainServer] Stopped safely";
}

void MainServer::setupCanThread(const QString &caninterface)
{
    m_canThread = new QThread(this);
    m_canThread->setObjectName("CAN_Thread");

    m_canWorker = new Can_worker_server(nullptr);
    m_canServer = new CanServer(m_canWorker, nullptr);

    m_canWorker->moveToThread(m_canThread);

#ifdef Q_OS_LINUX
    connect(m_canThread, &QThread::started, []() {
        bindThreadToCore(1, "CAN");
    });
#endif

    m_canThread->start();

    QMetaObject::invokeMethod(m_canWorker, [this, caninterface]() {
        m_canWorker->init(caninterface);
        qDebug() << "[MainServer] CAN Worker/Server initialized in thread:" << QThread::currentThread();
    }, Qt::QueuedConnection);

    if (m_udsWorker) {
        connect(m_udsWorker, &UDS_Worker::canResponseReady,
                m_canServer, &CanServer::sendCanResponse,
                Qt::QueuedConnection);
        connect(m_canServer, &CanServer::udsMessageReceived,
                m_udsWorker, &UDS_Worker::onCanUdsMessageReceived, Qt::QueuedConnection);
    }
}

void MainServer::setupTcpThread(quint16 tcpPort)
{
    m_tcpThread = new QThread(this);
    m_tcpThread->setObjectName("TCP_Thread");

    m_tcpWorkerServer = new TcpWorkerServer(nullptr);
    m_tcpWorkerServer->moveToThread(m_tcpThread);

#ifdef Q_OS_LINUX
    connect(m_tcpThread, &QThread::started, []() {
        bindThreadToCore(2, "TCP");
    });
#endif

    m_tcpThread->start();

    QMetaObject::invokeMethod(m_tcpWorkerServer, [this, tcpPort]() {
        m_tcpWorkerServer->initServer();
        m_tcpWorkerServer->startServer(tcpPort);
        qDebug() << "[MainServer] TCP Worker initialized in thread:" << QThread::currentThread();

        if (m_udsWorker) {
            connect(m_udsWorker, &UDS_Worker::tcpResponseReady,
                    m_tcpWorkerServer, &TcpWorkerServer::sendTcpResponse, Qt::QueuedConnection);
            connect(m_tcpWorkerServer, &TcpWorkerServer::udsMessageReceived,
                    m_udsWorker, &UDS_Worker::onTcpUdsMessageReceived, Qt::QueuedConnection);
        }

        connect(m_tcpWorkerServer, &TcpWorkerServer::clientConnected,
                this, &MainServer::onClientConnected);
        connect(m_tcpWorkerServer, &TcpWorkerServer::clientDisconnected,
                this, &MainServer::onClientDisconnected);
    }, Qt::QueuedConnection);
}

void MainServer::setupUdsThread()
{
    m_udsThread = new QThread(this);
    m_udsThread->setObjectName("UDS_Thread");

    m_udsWorker = new UDS_Worker(nullptr);
    m_udsWorker->moveToThread(m_udsThread);

#ifdef Q_OS_LINUX
    connect(m_udsThread, &QThread::started, []() {
        bindThreadToCore(3, "UDS");
    });
#endif

    m_udsThread->start();

    QMetaObject::invokeMethod(m_udsWorker, [this]() {
        m_udsWorker->init();
        qDebug() << "[MainServer] UDS Worker initialized in thread:" << QThread::currentThread();
        // 跨线程信号连接已在 setupTcpThread / setupCanThread 中完成，此处不重复
    }, Qt::QueuedConnection);
}

void MainServer::onClientConnected(QTcpSocket *client)
{
    qDebug() << "Client connected:" << client->peerAddress().toString();
}

void MainServer::onClientDisconnected(QTcpSocket *client)
{
    qDebug() << "Client disconnected:" << client->peerAddress().toString();
}
