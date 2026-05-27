#include "stresstestmonitor.h"
#include <QDebug>
StressTestMonitor::StressTestMonitor(QObject *parent)
    : QObject{parent},
    m_startFreq(10), m_maxFreq(100), m_stepFreq(10), m_currentFreq(10),m_testDurationSec(300)
{

    m_timer = nullptr;
    m_pressureTimer = nullptr;
    m_sendTimer = nullptr;
    m_stopTimer = nullptr;
}


void StressTestMonitor::setStressParameters(int startFreq,int maxFreq,int stepFreq,int stepIntervalMs){
    m_startFreq = startFreq;
    m_maxFreq = maxFreq;
    m_stepFreq = stepFreq;
    m_stepIntervalMs = stepIntervalMs;
}

void StressTestMonitor::setCanBitrate(int bitrate){
    m_bitrate=bitrate;
}
void StressTestMonitor::setTestDuration(int seconds)
{
    m_testDurationSec = seconds;
}
void StressTestMonitor::start(){
    qDebug() << "[压力测试] ========== 启动监控器 ==========";
    qDebug() << "[压力测试] 总时长:" << m_testDurationSec << "秒"
             << "| 起始:" << m_startFreq << "帧/秒"
             << "| 最大:" << m_maxFreq << "帧/秒"
             << "| 步进:" << m_stepFreq << "帧/秒"
             << "| 加压间隔:" << m_stepIntervalMs << "ms";

    // 清理旧定时器
    if (m_timer) { m_timer->stop(); delete m_timer; m_timer = nullptr; }
    if (m_pressureTimer) { m_pressureTimer->stop(); delete m_pressureTimer; m_pressureTimer = nullptr; }
    if (m_sendTimer) { m_sendTimer->stop(); delete m_sendTimer; m_sendTimer = nullptr; }

    // 重置数据
    m_totalBitsInTransit = 0;
    m_sentRequestCount = 0;
    m_receivedResponseCount = 0;
    m_lastTotalBits = 0;
    m_lastSentRequests = 0;
    m_lastReceivedResponses = 0;

    // 重置频率
    m_currentFreq = m_startFreq;

    // 创建定时器
    m_timer = new QTimer(this);
    m_pressureTimer = new QTimer(this);
    m_sendTimer = new QTimer(this);
    m_stopTimer = new QTimer(this);

    // 连接信号槽
    connect(m_timer, &QTimer::timeout, this, &StressTestMonitor::onTimerTimeout);
    connect(m_pressureTimer, &QTimer::timeout, this, &StressTestMonitor::onPressureIncreaseTimeout);
    connect(m_sendTimer, &QTimer::timeout, this, &StressTestMonitor::onSendFrameTimeout);
    connect(m_stopTimer, &QTimer::timeout, this, &StressTestMonitor::onTestFinished);
    // 设置间隔
    m_timer->setInterval(1000);
    m_pressureTimer->setInterval(m_stepIntervalMs);

    int startInterval = 1000 / m_currentFreq;
    if (startInterval < 1) startInterval = 1;
    m_sendTimer->setInterval(startInterval);

    m_stopTimer->setSingleShot(true);
    m_stopTimer->start(m_testDurationSec * 1000);

    qDebug() << "[压力测试] 起始频率:" << m_currentFreq
             << "帧/秒, 发送间隔:" << startInterval << "ms"
             << ", 加压间隔:" << m_stepIntervalMs << "ms";

    // 启动
    m_timer->start();
    m_pressureTimer->start();
    m_sendTimer->start();
}


void StressTestMonitor::stop()
{
    qDebug() << "[Monitor] stop() 开始";

    if (m_timer) {
        m_timer->stop();
        delete m_timer;
        m_timer = nullptr;
    }
    if (m_pressureTimer) {
        m_pressureTimer->stop();
        delete m_pressureTimer;
        m_pressureTimer = nullptr;
    }
    if (m_sendTimer) {
        m_sendTimer->stop();
        delete m_sendTimer;
        m_sendTimer = nullptr;
    }
    if (m_stopTimer) { m_stopTimer->stop(); delete m_stopTimer; m_stopTimer = nullptr; }
    qDebug() << "[Monitor] stop() 完成";
}

void StressTestMonitor::onTimerTimeout(){
    //计算时间差
    double timeDelta = m_timer->interval() / 1000.0;
    //计算总线负载率
    quint64 bitsThisSecond = m_totalBitsInTransit - m_lastTotalBits;
    qreal load = ( (bitsThisSecond / timeDelta) / m_bitrate) * 100.0;

    //计算丢包率（3 秒滑动窗口，消除请求-响应跨秒偏差）
    quint32 sentDelta = m_sentRequestCount - m_lastSentRequests;
    quint32 receivedDelta = m_receivedResponseCount - m_lastReceivedResponses;

    m_sentWindow.append(sentDelta);
    m_recvWindow.append(receivedDelta);
    if (m_sentWindow.size() > SLIDING_WINDOW_SIZE) {
        m_sentWindow.removeFirst();
        m_recvWindow.removeFirst();
    }

    quint32 sent3s = 0, recv3s = 0;
    for (int i = 0; i < m_sentWindow.size(); ++i) {
        sent3s += m_sentWindow[i];
        recv3s += m_recvWindow[i];
    }

    qreal lossRate = 0.0;
    if (sent3s > 0) {
        int lost = static_cast<int>(sent3s) - static_cast<int>(recv3s);
        lossRate = (qreal(qMax(0, lost)) / sent3s) * 100.0;
    }
    // 更新快照
    m_lastTotalBits = m_totalBitsInTransit;
    m_lastSentRequests = m_sentRequestCount;
    m_lastReceivedResponses = m_receivedResponseCount;
    // 发送信号给绘图界面
    emit statsUpdated(load, lossRate);
    qDebug() << "[Monitor] Emitting stats - Load:" << load << "%, Loss:" << lossRate << "%";
}

void StressTestMonitor::onTestFinished()
{
    qDebug() << "[压力测试] ========== 5分钟已到，自动停止 ==========";

    // 最后一次统计
    emit testFinished(m_totalBitsInTransit, m_sentRequestCount, m_receivedResponseCount);

    // 停止所有定时器
    stop();
}

void StressTestMonitor::onFrameSent(const QCanBusFrame &frame)
{
    // 只更新物理层的比特数，用于计算总线负载
    int frameBits = 40 + (frame.payload().size() * 8);
    m_totalBitsInTransit += frameBits;
}

void StressTestMonitor::onFrameReceived(const QCanBusFrame &frame)
{
    // 只更新物理层的比特数，用于计算总线负载
    int frameBits = 40 + (frame.payload().size() * 8);
    m_totalBitsInTransit += frameBits;
}

void StressTestMonitor::onRequestSent(const UDSMessage &data)
{
    // 只增加应用层的请求计数，用于计算丢包率
    Q_UNUSED(data);
    m_sentRequestCount++;
}

void StressTestMonitor::onResponseReceived(const UDSMessage &data)
{
    // 只增加应用层的响应计数，用于计算丢包率
    Q_UNUSED(data);
    m_receivedResponseCount++;
}

//加压控制
void StressTestMonitor::onPressureIncreaseTimeout(){
    if (m_currentFreq < m_maxFreq) {
        m_currentFreq += m_stepFreq;
        qDebug() << "[压力测试] 负载升级 -> 当前频率:" << m_currentFreq << "帧/秒";
        // 动态调整发送定时器的间隔
        // 频率越高，间隔越短
        int intervalMs = 1000 / m_currentFreq;
        if (intervalMs < 1) intervalMs = 1;
        m_sendTimer->setInterval(intervalMs);
    } else {
        // 达到最大压力，停止加压，保持最大频率运行
        m_pressureTimer->stop();
        qDebug() << "[压力测试] 已达到最大负载:" << m_maxFreq << "帧/秒";
    }
}

//发送数据
void StressTestMonitor::onSendFrameTimeout(){
    UDSMessage msg;

    msg.serviceId = 0x3E;
    msg.subFunction = 0x00;
    //msg.payload = QByteArray(50, 0xAA);
    emit requestToSendSignal(msg);
}
