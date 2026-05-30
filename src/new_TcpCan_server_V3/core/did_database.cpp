#include "did_database.h"
#include <QDebug>
#include <QMetaType>
#include <QtEndian>
#include <QFile>
#include <QProcess>

#define DID_CPU_TEMPERATURE     0xCC90   // CPU温度
#define DID_CPU_LOAD            0xCC91   // CPU负载
#define DID_MEMORY_USAGE        0xCC92   // 内存使用率
#define DID_SYSTEM_UPTIME       0xCC93   // 系统运行时间
#define DID_CAN_STATUS          0xCC94   // CAN接口状态


DID_database::DID_database(QObject *parent)
    : QObject{parent}
{
    initializeDefaultDatabase();
    initializeDynamicDIDs();
}


QVariant DID_database::getValue(uint16_t did) const
{
    QMutexLocker locker(&m_mutex); // 加锁
    if(m_dynamicDIDs.contains(did)){
        return readDynamicValue(did);
    }
    return m_database.value(did);
}

void DID_database::setValue(uint16_t did, const QVariant& value)
{
    QMutexLocker locker(&m_mutex); // 加锁
    m_database.insert(did, value);
    qDebug() << "[DID_DB] Updated DID:" << QString("0x%1").arg(did, 4, 16, QLatin1Char('0')) << "with value:" << value;
}

bool DID_database::contains(uint16_t did) const
{
    QMutexLocker locker(&m_mutex); // 加锁
    return m_database.contains(did)||m_dynamicDIDs.contains(did);
}

QList<uint16_t> DID_database::getAllDIDs() const
{
    QMutexLocker locker(&m_mutex); // 加锁
    QList<uint16_t> keys = m_database.keys();
    // 添加动态DID
    for (uint16_t did : m_dynamicDIDs) {
        if (!keys.contains(did)) {
            keys.append(did);
        }
    }
    return keys;
}

bool DID_database::tryGetValue(uint16_t did, QVariant& out_value) const
{
    QMutexLocker locker(&m_mutex); // 加锁
    if (m_dynamicDIDs.contains(did)) {
        out_value = readDynamicValue(did);
        return true;
    }

    auto it = m_database.find(did);
    if (it != m_database.end()) {
        out_value = it.value();
        return true;
    }
    return false;
}

bool DID_database::requiresAuthentication(uint16_t did) const
{
    //DID 0xF1xx 范围通常需要认证
    return (did >= 0xF100 && did <= 0xF1FF);
}

void DID_database::initializeDynamicDIDs()
{
    QMutexLocker locker(&m_mutex);

    m_dynamicDIDs.insert(DID_CPU_TEMPERATURE);
    m_dynamicDIDs.insert(DID_CPU_LOAD);
    m_dynamicDIDs.insert(DID_MEMORY_USAGE);
    m_dynamicDIDs.insert(DID_SYSTEM_UPTIME);
    m_dynamicDIDs.insert(DID_CAN_STATUS);

    qDebug() << "[DID_DB] Dynamic DIDs initialized:" << m_dynamicDIDs.size() << "entries";
}

QVariant DID_database::readDynamicValue(uint16_t did) const
{
    switch (did) {
    case DID_CPU_TEMPERATURE: return readCpuTemperature();
    case DID_CPU_LOAD:        return readCpuLoad();
    case DID_MEMORY_USAGE:    return readMemoryUsage();
    case DID_SYSTEM_UPTIME:   return readSystemUptime();
    case DID_CAN_STATUS:      return readCanStatus();
    default: return QVariant();
    }
}

QVariant DID_database::readCpuTemperature() const
{
    QFile file("/sys/class/thermal/thermal_zone0/temp");
    if (file.open(QIODevice::ReadOnly)) {
        int temp = file.readAll().trimmed().toInt() / 1000;
        file.close();
        qDebug() << "[DID_DB] CPU Temperature:" << temp << "°C";
        return QVariant(temp);
    }
    return QVariant(0);
}

QVariant DID_database::readCpuLoad() const
{
    QFile file("/proc/loadavg");
    if (file.open(QIODevice::ReadOnly)) {
        QString content = file.readAll();
        file.close();
        float load = content.split(' ').first().toFloat();
        qDebug() << "[DID_DB] CPU Load:" << load;
        return QVariant(load);
    }
    return QVariant(0.0f);
}

QVariant DID_database::readMemoryUsage() const
{
    QFile file("/proc/meminfo");
    if (file.open(QIODevice::ReadOnly)) {
        QString content = file.readAll();
        file.close();

        auto parseLine = [&](const QString& key) -> long {
            int start = content.indexOf(key);
            if (start < 0) return 0;
            int end = content.indexOf("kB", start);
            QString line = content.mid(start, end - start);
            return line.split(':').last().trimmed().toLong();
        };

        long total = parseLine("MemTotal");
        long available = parseLine("MemAvailable");

        if (total > 0) {
            int percent = (int)((total - available) * 100 / total);
            qDebug() << "[DID_DB] Memory Usage:" << percent << "%";
            return QVariant(percent);
        }
    }
    return QVariant(0);
}

QVariant DID_database::readSystemUptime() const
{
    QFile file("/proc/uptime");
    if (file.open(QIODevice::ReadOnly)) {
        QString content = file.readAll();
        file.close();
        float uptime = content.split(' ').first().toFloat();
        qDebug() << "[DID_DB] System Uptime:" << (uint32_t)uptime << "s";
        return QVariant((uint32_t)uptime);
    }
    return QVariant(0);
}

QVariant DID_database::readCanStatus() const
{
    QProcess process;
    process.start("ip", QStringList() << "-details" << "link" << "show" << "can0");
    process.waitForFinished(3000);
    QString output = process.readAllStandardOutput();

    QString status;
    if (output.contains("ERROR-ACTIVE")) {
        status = "ACTIVE";
    } else if (output.contains("ERROR-PASSIVE")) {
        status = "PASSIVE";
    } else if (output.contains("BUS-OFF")) {
        status = "BUS_OFF";
    } else if (output.contains("STOPPED")) {
        status = "STOPPED";
    } else {
        status = "UNKNOWN";
    }

    qDebug() << "[DID_DB] CAN Status:" << status;
    return QVariant(status);
}

void DID_database::initializeDefaultDatabase()
{
    QMutexLocker locker(&m_mutex); // 加锁

    // 车辆信息DID
    m_database.insert(DataIdentifier::VEHICLE_MANUFACTURER_SPARE_PART_NUMBER, "ABCD123456789");
    m_database.insert(DataIdentifier::VEHICLE_MANUFACTURER_ECU_SOFTWARE_VERSION_NUMBER, "V1.2.3");
    m_database.insert(DataIdentifier::APPLICATION_SOFTWARE_IDENTIFICATION, "v1.0");

    // 车身控制DID
    m_database.insert(DataIdentifier::CURRENT_VOLUME_LEVEL, 15); // 当前音量
    m_database.insert(DataIdentifier::MAX_VOLUME_LEVEL, 30);    // 最大音量
    m_database.insert(DataIdentifier::BATTERY_VOLTAGE, 12.4f); // 电池电压
    m_database.insert(DataIdentifier::CURRENT_TEMPERATURE, 24); // 当前温度
    m_database.insert(DataIdentifier::ENGINE_RPM, 0); // 发动机转速
    m_database.insert(DataIdentifier::VEHICLE_SPEED, 0); // 车速

    // 支持的服务列表
    m_database.insert(DataIdentifier::SUPPORTED_DIAGNOSTIC_SERVICES, "10 22 27 28 2E 31 3E");

    qDebug() << "[DID_DB] Default database initialized with" << m_database.size() << "entries";
}


