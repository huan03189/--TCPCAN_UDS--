#include "udshelper.h"

QByteArray udshelper::serializeUDSMessage(const UDSMessage&msg){
    QByteArray raw;

    raw.append(static_cast<char>(msg.serviceId));
    raw.append(static_cast<char>(msg.subFunction));
    raw.append(msg.payload);


    return raw;
}



UDSMessage udshelper::deserializeUDSMessage(const QByteArray&raw){
    UDSMessage msg;

    if(raw.isEmpty()){
        return msg;
    }

    //SID
    msg.serviceId=static_cast<quint8>(raw.at(0));

    //subFunction
    if(raw.size()>=2){
        msg.subFunction=static_cast<quint8>(raw.at(1));
    }


    //payload
    if(raw.size()>2){
        msg.payload=raw.mid(2);
    }
    //是否响应
    if(msg.serviceId>=0x40){
        msg.isResponse=true;
    }
    //负响应
    if(msg.serviceId==0x7F){
        msg.isNegativeResponse=true;

        if(raw.size()>=3){
            msg.negativeCode=static_cast<quint8>(raw.at(2));
        }
    }
    return msg;
}
