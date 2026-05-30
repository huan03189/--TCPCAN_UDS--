#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <QObject>
#include <QByteArray>
#include "types.h"
// TCP协议解析器
// 职责：
// 1. TCP粘包拆包
// 2. 包头校验
// 3. CRC校验
// 4. Type分发
class protocol_parser : public QObject
{
    Q_OBJECT
public:
    explicit protocol_parser(QObject *parent = nullptr);
    void pushData(const QByteArray &data);

signals:
    void canFrameReady(const CanFrameData &frame);

    void udsPacketReady(const QByteArray &udspacket);

    void parseError(const QString &msg);

private:
    QByteArray m_buffer;//缓冲区
};

#endif // PROTOCOL_PARSER_H
