-- ============================================================
-- TCPCAN UDS 诊断系统 — 数据库初始化脚本
-- 目标数据库: SQLite3 (server_logs.db)
-- 用法: sqlite3 server_logs.db < init_db.sql
-- ============================================================

-- 1. 通用事件日志
CREATE TABLE IF NOT EXISTS logs (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
    type        INTEGER NOT NULL,        -- 0=INFO 1=WARNING 2=ERROR 3=DEBUG 4=CAN_FRAME 5=CRC_ERROR 6=CLIENT_EVENT
    message     TEXT,
    details     TEXT
);
CREATE INDEX IF NOT EXISTS idx_logs_ts   ON logs(timestamp);
CREATE INDEX IF NOT EXISTS idx_logs_type ON logs(type);

-- 2. UDS 诊断事务记录
CREATE TABLE IF NOT EXISTS uds_transactions (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp       DATETIME DEFAULT CURRENT_TIMESTAMP,
    channel         TEXT    NOT NULL,     -- 'TCP' | 'CAN'
    client_id       INTEGER,             -- TCP 客户端 ID，CAN 通道为 NULL
    request_sid     INTEGER NOT NULL,    -- 请求服务 ID (0x10, 0x22, ...)
    request_subfn   INTEGER,             -- 子功能码
    request_did     INTEGER,             -- 数据标识符（0x22 服务）
    request_payload BLOB,                -- 请求数据
    response_sid    INTEGER,             -- 响应 SID（正响应 = SID+0x40，负响应 = 0x7F）
    nrc             INTEGER,             -- 否定响应码（正响应为 NULL）
    response_payload BLOB,               -- 响应数据
    latency_us      INTEGER              -- 处理延迟（微秒）
);
CREATE INDEX IF NOT EXISTS idx_uds_ts      ON uds_transactions(timestamp);
CREATE INDEX IF NOT EXISTS idx_uds_channel ON uds_transactions(channel);
CREATE INDEX IF NOT EXISTS idx_uds_sid     ON uds_transactions(request_sid);

-- 3. CAN 总线帧记录
CREATE TABLE IF NOT EXISTS can_frames (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
    direction   TEXT    NOT NULL,         -- 'RX' | 'TX'
    can_id      INTEGER NOT NULL,        -- CAN 仲裁 ID
    dlc         INTEGER NOT NULL,        -- 数据长度 (0-8)
    data        BLOB,                    -- 帧数据 (最多 8 字节)
    channel     TEXT    DEFAULT 'CAN'    -- 预留：多通道扩展
);
CREATE INDEX IF NOT EXISTS idx_can_ts      ON can_frames(timestamp);
CREATE INDEX IF NOT EXISTS idx_can_dir     ON can_frames(direction);

-- 4. 平台系统指标（定期采样）
CREATE TABLE IF NOT EXISTS system_metrics (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp       DATETIME DEFAULT CURRENT_TIMESTAMP,
    cpu_temp        REAL,                -- CPU 温度 (°C)
    cpu_load_1m     REAL,                -- 1 分钟平均负载
    cpu_load_5m     REAL,                -- 5 分钟平均负载
    mem_total_kb    INTEGER,             -- 总内存 (KB)
    mem_avail_kb    INTEGER,             -- 可用内存 (KB)
    mem_used_pct    REAL,                -- 内存使用百分比
    uptime_sec      INTEGER,             -- 系统运行时间 (秒)
    can_state       TEXT,                -- CAN 接口状态 (UP/DOWN)
    can_berrors     INTEGER              -- CAN 总线错误计数
);
CREATE INDEX IF NOT EXISTS idx_metrics_ts ON system_metrics(timestamp);

-- 5. 安全访问审计
CREATE TABLE IF NOT EXISTS security_audit (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp   DATETIME DEFAULT CURRENT_TIMESTAMP,
    channel     TEXT    NOT NULL,
    client_id   INTEGER,
    event       TEXT    NOT NULL,         -- 'SEED_REQUEST' | 'KEY_VALID' | 'KEY_INVALID'
    seed        INTEGER,                 -- 发送的种子值
    key_value   INTEGER,                 -- 客户端提供的密钥
    security_lvl INTEGER                 -- 安全等级
);
CREATE INDEX IF NOT EXISTS idx_sec_ts ON security_audit(timestamp);
