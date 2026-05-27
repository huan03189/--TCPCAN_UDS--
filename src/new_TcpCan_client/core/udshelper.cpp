#include "udshelper.h"
#include <QDebug>
QByteArray udshelper::serializeUDSMessage(const UDSMessage&msg){
    QByteArray raw;

    raw.append(static_cast<char>(msg.serviceId));
    raw.append(static_cast<char>(msg.subFunction));
    raw.append(msg.payload);


    return raw;
}



UDSMessage udshelper::deserializeUDSMessage(const QByteArray&raw){
    UDSMessage msg;

    if(raw.isEmpty())
        return msg;

    uint8_t sid = static_cast<uint8_t>(raw[0]);

    msg.isResponse = false;
    msg.isNegativeResponse = false;
    msg.negativeCode = 0;

    // 负响应
    if(sid == 0x7F){
        msg.isNegativeResponse = true;
        msg.isResponse = true;
        msg.serviceId = raw.size() >= 2 ? static_cast<uint8_t>(raw[1]) : 0;
        msg.subFunction = raw.size() >= 2 ? static_cast<uint8_t>(raw[1]) : 0;
        msg.negativeCode = raw.size() >= 3 ? static_cast<uint8_t>(raw[2]) : 0;
        msg.payload = raw.size() > 3 ? raw.mid(3) : QByteArray();
    }
    // 正响应
    else if(sid >= 0x40 && sid != 0x7F){
        msg.isResponse = true;
        msg.serviceId = sid;
        msg.subFunction = raw.size() >= 2 ? static_cast<uint8_t>(raw[1]) : 0;
        msg.payload = raw.size() > 2 ? raw.mid(2) : QByteArray();
    }
    // 请求
    else{
        msg.isResponse = false;
        msg.serviceId = sid;
        msg.subFunction = raw.size() >= 2 ? static_cast<uint8_t>(raw[1]) : 0;
        msg.payload = raw.size() > 2 ? raw.mid(2) : QByteArray();
    }

    return msg;
}
