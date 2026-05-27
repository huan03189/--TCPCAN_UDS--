#ifndef PROTOCOL_PARSER_H
#define PROTOCOL_PARSER_H

#include <QObject>
#include <QByteArray>
#include "types.h"
class protocol_parser : public QObject
{
    Q_OBJECT
public:
    explicit protocol_parser(QObject *parent = nullptr);

    void pushData(const QByteArray &data);

signals:
    void canFrameReady(const CanFrameData &frame);

    void udsMessageReady(const UDSMessage &response);//UDS原始数据信号

    void parseError(const QString &msg);


private:
    QByteArray m_buffer;//缓冲区
};

#endif // PROTOCOL_PARSER_H
