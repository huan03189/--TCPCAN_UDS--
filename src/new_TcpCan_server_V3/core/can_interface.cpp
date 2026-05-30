#include "can_interface.h"
#include "server/db_logger.h"
#include <QDebug>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <sys/time.h>
#include <time.h>
#include <QFile>
#include <QTextStream>
#include <QThread>

// 尝试将 CAN 控制器的中断绑定到指定 CPU 核心
static void trySetCanIrqAffinity(const QString& ifname, int targetCore) {
    // 1. 从 /proc/interrupts 找 CAN 控制器的中断号
    QFile irqFile("/proc/interrupts");
    if (!irqFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[IRQ Affinity] Cannot read /proc/interrupts, run as root to set IRQ affinity";
        qWarning() << "[IRQ Affinity] Manually: find CAN IRQ and run: echo <mask> > /proc/irq/<N>/smp_affinity";
        return;
    }

    int canIrq = -1;
    QTextStream stream(&irqFile);
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (line.contains(ifname, Qt::CaseInsensitive)) {
            QString irqStr = line.section(':', 0, 0).trimmed();
            bool ok = false;
            canIrq = irqStr.toInt(&ok);
            if (ok) break;
        }
    }
    irqFile.close();

    if (canIrq < 0) {
        qInfo() << "[IRQ Affinity] No IRQ found for interface" << ifname << ", skipping";
        return;
    }

    // 2. 计算 CPU 掩码
    int mask = 1 << targetCore;
    QString path = QString("/proc/irq/%1/smp_affinity").arg(canIrq);
    QFile affinityFile(path);
    if (!affinityFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[IRQ Affinity] Cannot write to" << path << "(needs root)";
        qWarning() << "[IRQ Affinity] Manually run: echo" << mask << ">" << path;
        return;
    }

    QTextStream out(&affinityFile);
    out << mask;
    affinityFile.close();

    qInfo() << "[IRQ Affinity] CAN IRQ" << canIrq << "bound to CPU core" << targetCore
            << "(mask:" << mask << ")";
}
#endif


can_interface_server* can_interface_server::m_instance=nullptr;

can_interface_server* can_interface_server::instance(){
    static can_interface_server instance;
    return &instance;
}

can_interface_server::can_interface_server(QObject *parent):QObject(parent){
#ifdef Q_OS_LINUX
    m_socketFd=-1;
    m_notifier=nullptr;
    m_writeNotifier=nullptr;
#endif
}

//析构函数
can_interface_server::~can_interface_server(){
#ifdef Q_OS_LINUX
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
    }
    if (m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
        delete m_writeNotifier;
    }
    if (m_socketFd >= 0) {
        ::close(m_socketFd);
    }
#endif
}


bool can_interface_server::init(const QString &interfacename){
#ifdef Q_OS_LINUX
    //创建Socket
    m_socketFd=::socket(PF_CAN,SOCK_RAW,CAN_RAW);
    if(m_socketFd<0){
        qCritical()<<"Error: Cannot create CAN socket:" << strerror(errno);
        DB_logger::instance()->log(DB_logger::LOG_ERROR,
                                   "CAN socket creation failed",
                                   strerror(errno));
        return false;
    }
    //获取接口索引
    struct ifreq ifr;
    strncpy(ifr.ifr_name, interfacename.toLocal8Bit().data(), IFNAMSIZ - 1);
    ifr.ifr_name[IFNAMSIZ - 1] = '\0'; // 确保字符串以 null 结尾

    int ret = ::ioctl(m_socketFd, SIOCGIFINDEX, &ifr);
    if (ret < 0) {
        qCritical() << "Error: Cannot find interface" << interfacename << ":" << strerror(errno);
        DB_logger::instance()->log(DB_logger::LOG_ERROR,
                                   QString("CAN interface not found: %1").arg(interfacename),
                                   strerror(errno));
        ::close(m_socketFd);
        return false;
    }
    //绑定Socket
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;

    ret = ::bind(m_socketFd, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        qCritical() << "Error: Cannot bind socket:" << strerror(errno);
        DB_logger::instance()->log(DB_logger::LOG_ERROR,
                                   "CAN socket bind failed",
                                   strerror(errno));
        ::close(m_socketFd);
        return false;
    }
    int sndbuf = 4 * 1024 * 1024;  // 4MB
    int rcvbuf = 4 * 1024 * 1024;
    if (setsockopt(m_socketFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
        qWarning() << "Failed to set SO_SNDBUF, using kernel default";
    }
    if (setsockopt(m_socketFd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf)) < 0) {
        qWarning() << "Failed to set SO_RCVBUF, using kernel default: might drop frames under load";
    }
    // 强制确保 socket 为阻塞模式（清零所有非阻塞标志）
    int curFlags = ::fcntl(m_socketFd, F_GETFL, 0);
    if (curFlags >= 0) {
        if (curFlags & O_NONBLOCK) {
            qWarning() << "Socket was NONBLOCK! Forcing to BLOCKING mode";
            ::fcntl(m_socketFd, F_SETFL, curFlags & ~O_NONBLOCK);
        }
        qInfo() << "Socket flags:" << QString("0x%1").arg(curFlags, 0, 16)
                << (curFlags & O_NONBLOCK ? "(NONBLOCK)" : "(BLOCKING)");
    }

    // Verify actual buffer size the kernel granted
    socklen_t optlen = sizeof(rcvbuf);
    if (getsockopt(m_socketFd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, &optlen) == 0) {
        qInfo() << "SO_RCVBUF:" << rcvbuf << "bytes =" << (rcvbuf / 16) << "CAN frames";
    }
    optlen = sizeof(sndbuf);
    if (getsockopt(m_socketFd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen) == 0) {
        qInfo() << "SO_SNDBUF:" << sndbuf << "bytes =" << (sndbuf / 16) << "CAN frames";
    }
    // 检查发送超时（阻塞模式下应为 0 = 无限等待）
    struct timeval sndtimeo;
    optlen = sizeof(sndtimeo);
    if (getsockopt(m_socketFd, SOL_SOCKET, SO_SNDTIMEO, &sndtimeo, &optlen) == 0) {
        qInfo() << "SO_SNDTIMEO:" << sndtimeo.tv_sec << "s" << sndtimeo.tv_usec << "ms"
                << (sndtimeo.tv_sec == 0 && sndtimeo.tv_usec == 0 ? "(block indefinitely)" : "(timeout set!)");
    }

    // 不设 O_NONBLOCK —— 读路径用 recv(MSG_DONTWAIT)，写路径保持阻塞

    //设置QSocketNotifier监听读取事件，当CAN总线有数据时，Qt会自动调用onSocketCanActivity
    m_notifier = new QSocketNotifier(m_socketFd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &can_interface_server::onSocketCanActivity);
    m_notifier->setEnabled(true);

    // 写通知器 — 默认禁用，TX 环形缓冲有数据时才开启
    m_writeNotifier = new QSocketNotifier(m_socketFd, QSocketNotifier::Write, this);
    connect(m_writeNotifier, &QSocketNotifier::activated, this, &can_interface_server::onSocketWriteReady);
    m_writeNotifier->setEnabled(false);

    // 尝试将 CAN 控制器中断绑定到 Core 1（与 CAN 线程同核，减少跨核开销）
    trySetCanIrqAffinity(interfacename, 1);

    qInfo() << "CAN Interface initialized on" << interfacename;

    DB_logger::instance()->log(DB_logger::LOG_INFO,
                               QString("CAN interface initialized: %1").arg(interfacename),
                               QString("RCVBUF: %1 bytes  SNDBUF: %2 bytes  IRQ: Core 1")
                                   .arg(rcvbuf).arg(sndbuf));

    return true;

#else
    //仅为了编译通过
    Q_UNUSED(interfacename)
    qWarning() << "Running on Windows: CAN init skipped.";
    return true;
#endif
}

void can_interface_server::transmit(const CanFrameData &frame){
#ifdef Q_OS_LINUX
    if(m_socketFd<0) {
        qWarning() << "CAN socket not initialized";
        return;
    }

    if (frame.len > CAN_MAX_DLEN) {
        qWarning() << "Frame data too large";
        return;
    }

    struct can_frame canFrame;
    memset(&canFrame, 0, sizeof(canFrame));
    canFrame.can_id = frame.id;
    canFrame.can_dlc = frame.len;
    memcpy(canFrame.data, frame.data, frame.len);

    // 先尝试直接发送（大多数时候直接成功）
    int len = ::write(m_socketFd, &canFrame, sizeof(struct can_frame));
    if (len == sizeof(struct can_frame))
        return;

    // 直接发送失败（驱动忙）→ 推入环形缓冲，由写通知器出队
    CanFrame bufFrame;
    bufFrame.id = frame.id;
    bufFrame.len = frame.len;
    memcpy(bufFrame.data, frame.data, frame.len);

    if (!m_txRingBuffer.push(bufFrame)) {
        qWarning() << "TX ring buffer full (512 slots)! ID:"
                   << QString("0x%1").arg(frame.id, 0, 16);
        DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                   "CAN TX ring buffer overflow",
                                   QString("Frame ID: 0x%1  DLC: %2").arg(frame.id, 0, 16).arg(frame.len));
        return;
    }

    // 仅在本线程启用写通知器；GUI线程调用时跳过，形成天然异步背压
    if (m_writeNotifier && !m_writeNotifier->isEnabled()
        && QThread::currentThread() == this->thread()) {
        m_writeNotifier->setEnabled(true);
    }
#endif
}

void can_interface_server::onSocketWriteReady(){
#ifdef Q_OS_LINUX
    for (int i = 0; i < 32; ++i) {
        CanFrame bufFrame;
        if (!m_txRingBuffer.pop(bufFrame))
            break;

        struct can_frame canFrame;
        memset(&canFrame, 0, sizeof(canFrame));
        canFrame.can_id = bufFrame.id;
        canFrame.can_dlc = bufFrame.len;
        memcpy(canFrame.data, bufFrame.data, bufFrame.len);

        int len;
        int retries = 0;
        do {
            len = ::write(m_socketFd, &canFrame, sizeof(canFrame));
            if (len == sizeof(canFrame))
                goto next_frame;
            if (errno == ENOBUFS && retries < 5) {
                struct timespec ts = {0, 100000};
                nanosleep(&ts, nullptr);
                retries++;
            } else {
                break;
            }
        } while (true);

        m_txRingBuffer.push(bufFrame);
        break;
next_frame:
        continue;
    }

    if (m_txRingBuffer.isEmpty() && m_writeNotifier) {
        m_writeNotifier->setEnabled(false);
    }
#endif
}

void can_interface_server::onSocketCanActivity(){
#ifdef Q_OS_LINUX

    // 每轮最多读取帧数，防止高负载下事件循环饥饿
    static const int MAX_FRAMES_PER_READ = 64;
    int framesRead = 0;

    while (framesRead < MAX_FRAMES_PER_READ) {
        struct can_frame canFrame;
        int nbytes = ::recv(m_socketFd, &canFrame, sizeof(struct can_frame), MSG_DONTWAIT);

        if (nbytes < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 缓冲区已清空，正常退出
                break;
            }
            qWarning() << "CAN Read error:" << strerror(errno);
            DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                       "CAN read error",
                                       strerror(errno));
            break;
        }

        if (nbytes < static_cast<int>(sizeof(struct can_frame))) {
            qWarning() << "Read: incomplete CAN frame";
            break;
        }

        // 确保 DLC 在有效范围内
        if (canFrame.can_dlc > CAN_MAX_DLEN) {
            qWarning() << "Frame DLC too large:" << canFrame.can_dlc;
            continue; // 跳过异常帧，继续读下一帧
        }

        CanFrameData myFrame;
        myFrame.id = canFrame.can_id;
        myFrame.len = canFrame.can_dlc;

        if (myFrame.len <= sizeof(myFrame.data)) {
            memcpy(myFrame.data, canFrame.data, myFrame.len);
            emit frameReceived(myFrame);
        } else {
            qWarning() << "Frame data too large to copy, length:" << myFrame.len;
        }

        ++framesRead;
    }

    if (framesRead >= MAX_FRAMES_PER_READ) {
        qWarning() << "Max frames per read reached (" << MAX_FRAMES_PER_READ
                   << "), more data may be pending";
    }

#endif
}

void can_interface_server::onWatchdogTimeout()
{
    // 此槽预留给未来内核级看门狗集成
}
