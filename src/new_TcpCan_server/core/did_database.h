#ifndef DID_DATABASE_H
#define DID_DATABASE_H

#include <QObject>
#include <QHash>
#include <QVariant>
#include <QMutex>
#include "types.h"
#include <QSet>
class DID_database : public QObject
{
    Q_OBJECT
public:
    explicit DID_database(QObject *parent = nullptr);

    //获取指定DID的值
    QVariant getValue(uint16_t did)const;

    //设置指定DID的值
    void setValue(uint16_t did, const QVariant& value);

    //检查DID是否存在
    bool contains(uint16_t did) const;

    // 获取所有 DID 列表
    QList<uint16_t> getAllDIDs() const;

    // 安全访问器：尝试获取值，并返回是否存在
    bool tryGetValue(uint16_t did, QVariant& out_value) const;

    // 检查 DID 是否需要认证才能访问
    bool requiresAuthentication(uint16_t did) const;

private:
    mutable QRecursiveMutex m_mutex; // 保护数据访问
    QHash<uint16_t, QVariant> m_database;

    //动态DID集合
     QSet<uint16_t> m_dynamicDIDs;

    //初始化默认数据库内容
    void initializeDefaultDatabase();

    //初始化动态DID
    void initializeDynamicDIDs();


    //动态读取函数
    QVariant readDynamicValue(uint16_t did) const;
    QVariant readCpuTemperature() const;
    QVariant readCpuLoad() const;
    QVariant readMemoryUsage() const;
    QVariant readSystemUptime() const;
    QVariant readCanStatus() const;
};

#endif // DID_DATABASE_H
