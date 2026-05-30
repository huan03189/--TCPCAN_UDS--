#ifndef DB_LOGGER_H
#define DB_LOGGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QDateTime>
#include <QMutex>
#include <QQueue>
#include <QTimer>
#include <QHash>

class DB_logger : public QObject
{
    Q_OBJECT
public:
    explicit DB_logger(QObject *parent = nullptr);
    ~DB_logger();

    // 日志等级（递增详细）
    enum LogLevel {
        LEVEL_SILENT  = 0,  // 不记录任何日志
        LEVEL_ERROR    = 1,  // 仅错误
        LEVEL_WARNING  = 2,  // 错误 + 警告
        LEVEL_INFO     = 3,  // + 一般信息（默认）
        LEVEL_DEBUG    = 4,  // + 调试信息
        LEVEL_TRACE    = 5   // + CAN 帧、TesterPresent 等高频事件
    };

    enum LogType {
        LOG_INFO        = 0,
        LOG_WARNING     = 1,
        LOG_ERROR       = 2,
        LOG_DEBUG       = 3,
        LOG_CAN_FRAME   = 4,
        LOG_CRC_ERROR   = 5,
        LOG_CLIENT_EVENT = 6
    };

    static DB_logger *instance();

    // 运行时等级控制
    void setLogLevel(LogLevel level);
    LogLevel logLevel() const;

    // 分类开关（独立于等级，精确控制）
    void setCategoryEnabled(const QString &category, bool enabled);
    bool isCategoryEnabled(const QString &category) const;

    // CAN 帧采样率（每 N 帧记录 1 帧，默认 N=1 全记录）
    void setCanFrameSampleRate(int rate);
    int canFrameSampleRate() const;

    // UDS 高频过滤：是否跳过 TesterPresent (0x3E)
    void setSkipTesterPresent(bool skip);
    bool skipTesterPresent() const;

    // 通用日志
    void log(LogType type, const QString &message, const QString &details = QString());
    void logClientEvent(const QString &event, const QString &clientInfo);

    // UDS 事务日志
    void logUdsTransaction(const QString &channel, int clientId,
                           uint8_t requestSid, uint8_t requestSubFn,
                           uint16_t requestDid, const QByteArray &requestPayload,
                           uint8_t responseSid, uint8_t nrc,
                           const QByteArray &responsePayload, qint64 latencyUs);

    // CAN 帧日志（内部采样）
    void logCanFrame(const QString &direction, quint32 canId,
                     quint8 dlc, const QByteArray &data);

    // 系统指标日志
    void logSystemMetric(double cpuTemp, double cpuLoad1m, double cpuLoad5m,
                         qint64 memTotalKb, qint64 memAvailKb, double memUsedPct,
                         qint64 uptimeSec, const QString &canState, int canBerrors);

    // 安全访问审计
    void logSecurityAudit(const QString &channel, int clientId,
                          const QString &event, quint32 seed,
                          quint32 keyValue, quint8 securityLvl);

    void flush();

private slots:
    void flushLogQueue();

private:
    void initializeDatabase();

    // 异步队列条目类型
    enum EntryType {
        ENTRY_LOG,
        ENTRY_UDS,
        ENTRY_CAN,
        ENTRY_METRIC,
        ENTRY_SECURITY
    };

    struct LogEntry {
        EntryType entryType;
        // ENTRY_LOG
        LogType logType;
        QString message;
        QString details;
        // ENTRY_UDS
        QString udsChannel;
        int udsClientId;
        uint8_t udsRequestSid;
        uint8_t udsRequestSubFn;
        uint16_t udsRequestDid;
        QByteArray udsRequestPayload;
        uint8_t udsResponseSid;
        uint8_t udsNrc;
        QByteArray udsResponsePayload;
        qint64 udsLatencyUs;
        // ENTRY_CAN
        QString canDirection;
        quint32 canId;
        quint8 canDlc;
        QByteArray canData;
        // ENTRY_METRIC
        double metricCpuTemp;
        double metricCpuLoad1m;
        double metricCpuLoad5m;
        qint64 metricMemTotalKb;
        qint64 metricMemAvailKb;
        double metricMemUsedPct;
        qint64 metricUptimeSec;
        QString metricCanState;
        int metricCanBerrors;
        // ENTRY_SECURITY
        QString secChannel;
        int secClientId;
        QString secEvent;
        quint32 secSeed;
        quint32 secKeyValue;
        quint8 secSecurityLvl;
    };

    void enqueueEntry(const LogEntry &entry);

    // 各表写入
    void insertLog(LogType type, const QString &message, const QString &details);
    void insertUdsTransaction(const LogEntry &e);
    void insertCanFrame(const LogEntry &e);
    void insertSystemMetric(const LogEntry &e);
    void insertSecurityAudit(const LogEntry &e);

    // 过滤判断
    bool shouldLogLevel(LogLevel required) const;
    bool shouldLogCategory(const QString &category) const;

    QSqlDatabase m_db;

    QQueue<LogEntry> m_logQueue;
    QMutex m_queueMutex;
    QTimer *m_flushTimer;

    LogLevel m_logLevel;
    QHash<QString, bool> m_categoryFilters;
    int m_canSampleRate;
    int m_canFrameCounter;          // CAN 帧计数器（采样用）
    bool m_skipTesterPresent;

    static DB_logger *m_instance;
};

#endif // DB_LOGGER_H
