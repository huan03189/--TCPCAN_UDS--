#ifndef UDSHELPER_H
#define UDSHELPER_H


#include "types.h"
class udshelper
{
public:

    static QByteArray serializeUDSMessage(const UDSMessage& msg);

    static UDSMessage deserializeUDSMessage(const QByteArray& raw);
};

#endif // UDSHELPER_H
