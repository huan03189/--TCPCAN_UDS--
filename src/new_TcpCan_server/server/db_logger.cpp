#include "db_logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QSqlError>
#include <QDebug>
#include <QCoreApplication>

DB_logger *DB_logger::m_instance = nullptr;

DB_logger *DB_logger::instance()
{
    if (m_instance == nullptr) {
        m_instance = new DB_logger();
    }
    return m_instance;
}

DB_logger::DB_logger(QObject *parent)
    : QObject{parent}
    , m_flushTimer(new QTimer(this))
    , m_logLevel(LEVEL_INFO)
    , m_canSampleRate(1)
    , m_canFrameCounter(0)
    , m_skipTesterPresent(false)
{
    initializeDatabase();

    connect(m_flushTimer, &QTimer::timeout, this, &DB_logger::flushLogQueue);
    m_flushTimer->start(5000);
}

DB_logger::~DB_logger()
{
    m_flushTimer->stop();
    flushLogQueue();
    m_db.close();
    m_instance = nullptr;
}

// ── 运行时等级控制 ──────────────────────────────────

void DB_logger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
    qInfo() << "[DB_logger] Log level set to:" << level;
}

DB_logger::LogLevel DB_logger::logLevel() const
{
    return m_logLevel;
}

void DB_logger::setCategoryEnabled(const QString &category, bool enabled)
{
    m_categoryFilters[category] = enabled;
    qInfo() << "[DB_logger] Category" << category << (enabled ? "enabled" : "disabled");
}

bool DB_logger::isCategoryEnabled(const QString &category) const
{
    return m_categoryFilters.value(category, true); // 默认开启
}

void DB_logger::setCanFrameSampleRate(int rate)
{
    m_canSampleRate = qMax(1, rate);
    qInfo() << "[DB_logger] CAN frame sample rate: 1/" << m_canSampleRate;
}

int DB_logger::canFrameSampleRate() const
{
    return m_canSampleRate;
}

void DB_logger::setSkipTesterPresent(bool skip)
{
    m_skipTesterPresent = skip;
    qInfo() << "[DB_logger] Skip TesterPresent:" << (skip ? "ON" : "OFF");
}

bool DB_logger::skipTesterPresent() const
{
    return m_skipTesterPresent;
}

// ── 过滤判断 ────────────────────────────────────────

bool DB_logger::shouldLogLevel(LogLevel required) const
{
    return m_logLevel >= required;
}

bool DB_logger::shouldLogCategory(const QString &category) const
{
    return m_categoryFilters.value(category, true);
}

// ── 通用日志 ────────────────────────────────────────

void DB_logger::log(LogType type, const QString &message, const QString &details)
{
    // 根据 type 推断需要的等级
    LogLevel required;
    switch (type) {
    case LOG_ERROR:
    case LOG_CRC_ERROR:
        required = LEVEL_ERROR;
        break;
    case LOG_WARNING:
        required = LEVEL_WARNING;
        break;
    case LOG_INFO:
    case LOG_CLIENT_EVENT:
        required = LEVEL_INFO;
        break;
    case LOG_DEBUG:
        required = LEVEL_DEBUG;
        break;
    case LOG_CAN_FRAME:
        required = LEVEL_TRACE;
        break;
    default:
        required = LEVEL_INFO;
        break;
    }

    if (!shouldLogLevel(required)) return;

    LogEntry entry;
    entry.entryType = ENTRY_LOG;
    entry.logType   = type;
    entry.message   = message;
    entry.details   = details;
    enqueueEntry(entry);
}

void DB_logger::logClientEvent(const QString &event, const QString &clientInfo)
{
    log(LOG_CLIENT_EVENT, QString("Client Event: %1").arg(event), clientInfo);
}

// ── UDS 事务日志 ────────────────────────────────────

void DB_logger::logUdsTransaction(const QString &channel, int clientId,
                                   uint8_t requestSid, uint8_t requestSubFn,
                                   uint16_t requestDid, const QByteArray &requestPayload,
                                   uint8_t responseSid, uint8_t nrc,
                                   const QByteArray &responsePayload, qint64 latencyUs)
{
    static const uint8_t SID_TESTER_PRESENT = 0x3E;

    // TesterPresent 过滤
    if (m_skipTesterPresent && requestSid == SID_TESTER_PRESENT)
        return;

    // TesterPresent 仅在 TRACE 级别记录
    if (requestSid == SID_TESTER_PRESENT && !shouldLogLevel(LEVEL_TRACE))
        return;

    // 其他 UDS 事务需要 INFO 级别
    if (!shouldLogLevel(LEVEL_INFO)) return;

    if (!shouldLogCategory("uds")) return;

    LogEntry entry;
    entry.entryType       = ENTRY_UDS;
    entry.udsChannel      = channel;
    entry.udsClientId     = clientId;
    entry.udsRequestSid   = requestSid;
    entry.udsRequestSubFn = requestSubFn;
    entry.udsRequestDid   = requestDid;
    entry.udsRequestPayload = requestPayload;
    entry.udsResponseSid  = responseSid;
    entry.udsNrc          = nrc;
    entry.udsResponsePayload = responsePayload;
    entry.udsLatencyUs    = latencyUs;
    enqueueEntry(entry);
}

// ── CAN 帧日志（带采样） ────────────────────────────

void DB_logger::logCanFrame(const QString &direction, quint32 canId,
                             quint8 dlc, const QByteArray &data)
{
    if (!shouldLogLevel(LEVEL_TRACE)) return;
    if (!shouldLogCategory("can")) return;

    // 采样：每 N 帧记录 1 帧
    m_canFrameCounter++;
    if (m_canFrameCounter % m_canSampleRate != 0)
        return;

    LogEntry entry;
    entry.entryType    = ENTRY_CAN;
    entry.canDirection = direction;
    entry.canId        = canId;
    entry.canDlc       = dlc;
    entry.canData      = data;
    enqueueEntry(entry);
}

// ── 系统指标日志 ────────────────────────────────────

void DB_logger::logSystemMetric(double cpuTemp, double cpuLoad1m, double cpuLoad5m,
                                 qint64 memTotalKb, qint64 memAvailKb, double memUsedPct,
                                 qint64 uptimeSec, const QString &canState, int canBerrors)
{
    if (!shouldLogLevel(LEVEL_INFO)) return;
    if (!shouldLogCategory("metric")) return;

    LogEntry entry;
    entry.entryType         = ENTRY_METRIC;
    entry.metricCpuTemp     = cpuTemp;
    entry.metricCpuLoad1m   = cpuLoad1m;
    entry.metricCpuLoad5m   = cpuLoad5m;
    entry.metricMemTotalKb  = memTotalKb;
    entry.metricMemAvailKb  = memAvailKb;
    entry.metricMemUsedPct  = memUsedPct;
    entry.metricUptimeSec   = uptimeSec;
    entry.metricCanState    = canState;
    entry.metricCanBerrors  = canBerrors;
    enqueueEntry(entry);
}

// ── 安全访问审计 ────────────────────────────────────

void DB_logger::logSecurityAudit(const QString &channel, int clientId,
                                  const QString &event, quint32 seed,
                                  quint32 keyValue, quint8 securityLvl)
{
    if (!shouldLogLevel(LEVEL_INFO)) return;
    if (!shouldLogCategory("security")) return;

    LogEntry entry;
    entry.entryType      = ENTRY_SECURITY;
    entry.secChannel     = channel;
    entry.secClientId    = clientId;
    entry.secEvent       = event;
    entry.secSeed        = seed;
    entry.secKeyValue    = keyValue;
    entry.secSecurityLvl = securityLvl;
    enqueueEntry(entry);
}

// ── 异步队列 ────────────────────────────────────────

void DB_logger::enqueueEntry(const LogEntry &entry)
{
    QMutexLocker locker(&m_queueMutex);
    m_logQueue.enqueue(entry);
}

void DB_logger::flush()
{
    QMetaObject::invokeMethod(this, "flushLogQueue", Qt::QueuedConnection);
}

// ── 队列刷新（定时器驱动） ──────────────────────────

void DB_logger::flushLogQueue()
{
    QMutexLocker locker(&m_queueMutex);

    while (!m_logQueue.isEmpty()) {
        const LogEntry &entry = m_logQueue.head();

        switch (entry.entryType) {
        case ENTRY_LOG:
            insertLog(entry.logType, entry.message, entry.details);
            break;
        case ENTRY_UDS:
            insertUdsTransaction(entry);
            break;
        case ENTRY_CAN:
            insertCanFrame(entry);
            break;
        case ENTRY_METRIC:
            insertSystemMetric(entry);
            break;
        case ENTRY_SECURITY:
            insertSecurityAudit(entry);
            break;
        }

        m_logQueue.dequeue();
    }
}

// ── 数据库初始化 ────────────────────────────────────

void DB_logger::initializeDatabase()
{
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dbPath);
    dbPath += "/server_logs.db";

    m_db = QSqlDatabase::addDatabase("QSQLITE", "logger_connection");
    m_db.setDatabaseName(dbPath);

    if (!m_db.open()) {
        qWarning() << "Failed to open log database:" << m_db.lastError().text();
        return;
    }

    // WAL 模式：读写并发更好，适合压力场景
    {
        QSqlQuery pragma(m_db);
        pragma.exec("PRAGMA journal_mode=WAL");
        pragma.exec("PRAGMA synchronous=NORMAL");
        pragma.exec("PRAGMA cache_size=-8000");  // 8MB 缓存
    }

    QSqlQuery query(m_db);

    // 1. 通用事件日志
    query.exec("CREATE TABLE IF NOT EXISTS logs ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "type INTEGER,"
               "message TEXT,"
               "details TEXT"
               ")");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_ts   ON logs(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_logs_type ON logs(type)");

    // 2. UDS 诊断事务
    query.exec("CREATE TABLE IF NOT EXISTS uds_transactions ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "channel TEXT NOT NULL,"
               "client_id INTEGER,"
               "request_sid INTEGER NOT NULL,"
               "request_subfn INTEGER,"
               "request_did INTEGER,"
               "request_payload BLOB,"
               "response_sid INTEGER,"
               "nrc INTEGER,"
               "response_payload BLOB,"
               "latency_us INTEGER"
               ")");
    query.exec("CREATE INDEX IF NOT EXISTS idx_uds_ts      ON uds_transactions(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_uds_channel ON uds_transactions(channel)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_uds_sid     ON uds_transactions(request_sid)");

    // 3. CAN 总线帧
    query.exec("CREATE TABLE IF NOT EXISTS can_frames ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "direction TEXT NOT NULL,"
               "can_id INTEGER NOT NULL,"
               "dlc INTEGER NOT NULL,"
               "data BLOB"
               ")");
    query.exec("CREATE INDEX IF NOT EXISTS idx_can_ts  ON can_frames(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_can_dir ON can_frames(direction)");

    // 4. 系统指标
    query.exec("CREATE TABLE IF NOT EXISTS system_metrics ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "cpu_temp REAL,"
               "cpu_load_1m REAL,"
               "cpu_load_5m REAL,"
               "mem_total_kb INTEGER,"
               "mem_avail_kb INTEGER,"
               "mem_used_pct REAL,"
               "uptime_sec INTEGER,"
               "can_state TEXT,"
               "can_berrors INTEGER"
               ")");
    query.exec("CREATE INDEX IF NOT EXISTS idx_metrics_ts ON system_metrics(timestamp)");

    // 5. 安全访问审计
    query.exec("CREATE TABLE IF NOT EXISTS security_audit ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT,"
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,"
               "channel TEXT NOT NULL,"
               "client_id INTEGER,"
               "event TEXT NOT NULL,"
               "seed INTEGER,"
               "key_value INTEGER,"
               "security_lvl INTEGER"
               ")");
    query.exec("CREATE INDEX IF NOT EXISTS idx_sec_ts ON security_audit(timestamp)");

    qInfo() << "Database initialized at:" << dbPath;
}

// ── 各表 INSERT（在 flushLogQueue 中调用） ─────────

void DB_logger::insertLog(LogType type, const QString &message, const QString &details)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO logs (type, message, details) VALUES (?, ?, ?)");
    query.addBindValue(type);
    query.addBindValue(message);
    query.addBindValue(details);

    if (!query.exec()) {
        qWarning() << "Failed to insert log:" << query.lastError().text();
    }
}

void DB_logger::insertUdsTransaction(const LogEntry &e)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO uds_transactions "
                  "(channel, client_id, request_sid, request_subfn, request_did, "
                  "request_payload, response_sid, nrc, response_payload, latency_us) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(e.udsChannel);
    query.addBindValue(e.udsClientId >= 0 ? e.udsClientId : QVariant());
    query.addBindValue(e.udsRequestSid);
    query.addBindValue(e.udsRequestSubFn);
    query.addBindValue(e.udsRequestDid > 0 ? e.udsRequestDid : QVariant());
    query.addBindValue(e.udsRequestPayload);
    query.addBindValue(e.udsResponseSid);
    query.addBindValue(e.udsNrc != 0x00 ? e.udsNrc : QVariant());
    query.addBindValue(e.udsResponsePayload);
    query.addBindValue(e.udsLatencyUs);

    if (!query.exec()) {
        qWarning() << "Failed to insert UDS transaction:" << query.lastError().text();
    }
}

void DB_logger::insertCanFrame(const LogEntry &e)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO can_frames (direction, can_id, dlc, data) VALUES (?, ?, ?, ?)");
    query.addBindValue(e.canDirection);
    query.addBindValue(e.canId);
    query.addBindValue(e.canDlc);
    query.addBindValue(e.canData);

    if (!query.exec()) {
        qWarning() << "Failed to insert CAN frame:" << query.lastError().text();
    }
}

void DB_logger::insertSystemMetric(const LogEntry &e)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO system_metrics "
                  "(cpu_temp, cpu_load_1m, cpu_load_5m, mem_total_kb, mem_avail_kb, "
                  "mem_used_pct, uptime_sec, can_state, can_berrors) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(e.metricCpuTemp);
    query.addBindValue(e.metricCpuLoad1m);
    query.addBindValue(e.metricCpuLoad5m);
    query.addBindValue(e.metricMemTotalKb);
    query.addBindValue(e.metricMemAvailKb);
    query.addBindValue(e.metricMemUsedPct);
    query.addBindValue(e.metricUptimeSec);
    query.addBindValue(e.metricCanState);
    query.addBindValue(e.metricCanBerrors);

    if (!query.exec()) {
        qWarning() << "Failed to insert system metric:" << query.lastError().text();
    }
}

void DB_logger::insertSecurityAudit(const LogEntry &e)
{
    QSqlQuery query(m_db);
    query.prepare("INSERT INTO security_audit "
                  "(channel, client_id, event, seed, key_value, security_lvl) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(e.secChannel);
    query.addBindValue(e.secClientId >= 0 ? e.secClientId : QVariant());
    query.addBindValue(e.secEvent);
    query.addBindValue(e.secSeed > 0 ? e.secSeed : QVariant());
    query.addBindValue(e.secKeyValue > 0 ? e.secKeyValue : QVariant());
    query.addBindValue(e.secSecurityLvl);

    if (!query.exec()) {
        qWarning() << "Failed to insert security audit:" << query.lastError().text();
    }
}
