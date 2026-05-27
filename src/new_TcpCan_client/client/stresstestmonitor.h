#ifndef STRESSTESTMONITOR_H
#define STRESSTESTMONITOR_H

#include <QObject>
#include <QTimer>
#include <QCanBusFrame>
#include "core/types.h"
class StressTestMonitor : public QObject
{
    Q_OBJECT
public:
    explicit StressTestMonitor(QObject *parent = nullptr);

    void setCanBitrate(int bitrate);//波特率

    //设置加压策略：起始频率，最大频率，每次增加的步长，增加步长的间隔
    void setStressParameters(int startFreq,int maxFreq,int stepFreq,int stepIntervalMs);

public slots:
    void onTimerTimeout();

    //监听客户端发送的can帧
     void onFrameSent(const QCanBusFrame &frame);

    //监听客户端接收的can帧
    void onFrameReceived(const QCanBusFrame &frame);

    //监听客户端发送的请求
    void onRequestSent(const UDSMessage &data);

    //监听客户端收到的响应
    void onResponseReceived(const UDSMessage &data);

    void setTestDuration(int seconds);

    void start();
    void stop();

signals:
    //每隔一秒发送一次统计结果
    void statsUpdated(qreal busLoad,qreal racketLossRate);

    //请求发送数据的信号
    void requestToSendSignal(const UDSMessage& data);

    void testFinished(quint64 totalBits, quint32 sentCount, quint32 receivedCount);

private slots:
    //压力控制逻辑
    void onPressureIncreaseTimeout();

    //发送动作逻辑
    void onSendFrameTimeout();

    void onTestFinished();

private:
    QTimer *m_timer;

    QTimer *m_pressureTimer;//加压控制定时器

    QTimer *m_sendTimer;//实际发送定时器

    QTimer *m_stopTimer;

    //压力策略参数
    int m_startFreq;//初始频率
    int m_maxFreq;//最大频率
    int m_stepFreq;//每次增加的频率
    int m_currentFreq;//当前频率
    int m_stepIntervalMs;//保存间隔时间
    int m_testDurationSec;


    int m_bitrate=500000;


    // 统计变量
    quint64 m_totalBitsInTransit = 0; // 总比特数（发送 + 接收，用于负载）
    quint32 m_sentRequestCount = 0;   // 发送的请求数
    quint32 m_receivedResponseCount = 0; // 接收到的响应数

    // 上一周期的统计快照（用于计算速率）
    quint64 m_lastTotalBits = 0;
    quint32 m_lastSentRequests = 0;
    quint32 m_lastReceivedResponses = 0;

    // 3 秒滑动窗口：消除采样偏差导致的假丢包
    QList<quint32> m_sentWindow;
    QList<quint32> m_recvWindow;
    static const int SLIDING_WINDOW_SIZE = 3;
};

#endif // STRESSTESTMONITOR_H
