#include "uds_handler.h"
#include "types.h"
#include <QDebug>
#include <QDateTime>
#include <QByteArray>
#include <QCryptographicHash>
#include <QMetaType>
#include <QtEndian>
#include <QRandomGenerator>

UDS_handler::UDS_handler(QObject *parent)
    : QObject{parent}
{
    m_didDatabase = new DID_database(this);
}

UDSMessage UDS_handler::processRequest(const UDSMessage &req)
{
    uint8_t sid = req.serviceId;

    // 检查会话要求（部分服务需要特定会话）
    if (sid != UDS_SID::DIAGNOSTIC_SESSION_CONTROL &&
        sid != UDS_SID::TESTER_PRESENT &&
        sid != UDS_SID::SECURITY_ACCESS) {
        if (m_session.currentSession == DiagnosticSession::DEFAULT_SESSION) {
            if (sid == UDS_SID::READ_DATA_BY_IDENTIFIER ||
                sid == UDS_SID::WRITE_DATA_BY_IDENTIFIER ||
                sid == UDS_SID::ROUTINE_CONTROL) {
                return makeNegativeResponse(sid, NegativeResponseCode::SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION);
            }
        }
    }

    switch (sid) {
    case UDS_SID::DIAGNOSTIC_SESSION_CONTROL:
        return handleDiagnosticSessionControl(req);
    case UDS_SID::READ_DATA_BY_IDENTIFIER:
        return handleReadDataByIdentifier(req);
    case UDS_SID::TESTER_PRESENT:
        return handleTesterPresent(req);
    case UDS_SID::SECURITY_ACCESS:
        return handleSecurityAccess(req);
    case UDS_SID::ECU_RESET:
        return handleECUReset(req);
    default:
        return makeNegativeResponse(sid, NegativeResponseCode::SERVICE_NOT_SUPPORTED);
    }
}

UDSMessage UDS_handler::handleDiagnosticSessionControl(const UDSMessage &req)
{
    uint8_t subFunction = req.subFunction;
    uint8_t oldSession = m_session.currentSession;

    switch (subFunction) {
    case DiagnosticSession::DEFAULT_SESSION:
        m_session.currentSession = DiagnosticSession::DEFAULT_SESSION;
        m_session.isAuthenticated = false;
        m_session.securityLevel = 0;
        break;
    case DiagnosticSession::PROGRAMMING_SESSION:
        if (m_session.isAuthenticated) {
            m_session.currentSession = DiagnosticSession::PROGRAMMING_SESSION;
        } else {
            return makeNegativeResponse(UDS_SID::DIAGNOSTIC_SESSION_CONTROL,
                                        NegativeResponseCode::SECURITY_ACCESS_DENIED);
        }
        break;
    case DiagnosticSession::EXTENDED_DIAGNOSTIC:
        if (m_session.isAuthenticated) {
            m_session.currentSession = DiagnosticSession::EXTENDED_DIAGNOSTIC;
        } else {
            return makeNegativeResponse(UDS_SID::DIAGNOSTIC_SESSION_CONTROL,
                                        NegativeResponseCode::SECURITY_ACCESS_DENIED);
        }
        break;
    default:
        return makeNegativeResponse(UDS_SID::DIAGNOSTIC_SESSION_CONTROL,
                                    NegativeResponseCode::SUB_FUNCTION_NOT_SUPPORTED);
    }

    if (oldSession != m_session.currentSession) {
        qDebug() << "[UDS] Session changed: 0x" + QString::number(oldSession, 16)
                 << "-> 0x" + QString::number(m_session.currentSession, 16);
    }

    UDSMessage msg;
    msg.serviceId = UDS_SID::DIAGNOSTIC_SESSION_CONTROL + UDS_SID::POSITIVE_RESPONSE_MASK;
    msg.subFunction = subFunction;
    msg.payload.append(static_cast<char>(0x00));
    msg.isResponse = true;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

UDSMessage UDS_handler::handleReadDataByIdentifier(const UDSMessage &req)
{
    if (req.payload.size() < 2) {
        return makeNegativeResponse(UDS_SID::READ_DATA_BY_IDENTIFIER,
                                    NegativeResponseCode::INCORRECT_MESSAGE_LENGTH);
    }

    uint16_t did = qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(req.payload.constData()));

    if (!m_didDatabase->contains(did)) {
        return makeNegativeResponse(UDS_SID::READ_DATA_BY_IDENTIFIER,
                                    NegativeResponseCode::REQUEST_OUT_OF_RANGE);
    }

    if (did >= 0xF1A0 && !m_session.isAuthenticated) {
        return makeNegativeResponse(UDS_SID::READ_DATA_BY_IDENTIFIER,
                                    NegativeResponseCode::SECURITY_ACCESS_DENIED);
    }

    QVariant value = m_didDatabase->getValue(did);
    QByteArray dataBytes = convertVariantToBytes(value);

    UDSMessage msg;
    msg.serviceId = UDS_SID::READ_DATA_BY_IDENTIFIER + UDS_SID::POSITIVE_RESPONSE_MASK;
    msg.subFunction = 0x00;
    msg.payload.append(did & 0xFF);
    msg.payload.append((did >> 8) & 0xFF);
    msg.payload.append(dataBytes);
    msg.isResponse = true;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

QByteArray UDS_handler::convertVariantToBytes(const QVariant &value)
{
    QByteArray bytes;

    if (value.type() == QMetaType::QString) {
        bytes = value.toString().toUtf8();
    } else if (value.type() == QMetaType::Int) {
        int val = value.toInt();
        quint32 valLe = qToLittleEndian<quint32>(static_cast<quint32>(val));
        bytes.append(reinterpret_cast<const char *>(&valLe), 4);
    } else if (value.type() == QMetaType::Float || value.type() == QMetaType::Double) {
        float val = value.toFloat();
        union { float f; quint32 i; } u;
        u.f = val;
        quint32 u32ValLe = qToLittleEndian<quint32>(u.i);
        bytes.append(reinterpret_cast<const char *>(&u32ValLe), 4);
    } else if (value.type() == QMetaType::QByteArray) {
        bytes = value.toByteArray();
    } else {
        bytes = value.toString().toUtf8();
    }

    return bytes;
}

UDSMessage UDS_handler::makeNegativeResponse(uint8_t sid, uint8_t nrc)
{
    UDSMessage msg;
    msg.serviceId = 0x7F;
    msg.subFunction = sid;
    msg.negativeCode = nrc;
    msg.isNegativeResponse = true;
    msg.isResponse = true;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

UDSMessage UDS_handler::makePositiveResponse(uint8_t sid, const QByteArray &payload)
{
    UDSMessage msg;
    msg.serviceId = sid + UDS_SID::POSITIVE_RESPONSE_MASK;
    msg.subFunction = 0x00;
    msg.payload = payload;
    msg.isResponse = true;
    msg.isNegativeResponse = false;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

UDSMessage UDS_handler::handleTesterPresent(const UDSMessage &req)
{
    uint8_t subFunction = req.subFunction;
    if (subFunction != 0x00) {
        return makeNegativeResponse(UDS_SID::TESTER_PRESENT,
                                    NegativeResponseCode::SUB_FUNCTION_NOT_SUPPORTED);
    }
    UDSMessage msg;
    msg.serviceId = UDS_SID::TESTER_PRESENT + UDS_SID::POSITIVE_RESPONSE_MASK;
    msg.subFunction = 0x00;
    msg.isResponse = true;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

UDSMessage UDS_handler::handleSecurityAccess(const UDSMessage &req)
{
    uint8_t subFunction = req.subFunction;
    if (subFunction % 2 == 1) { // 请求种子
        m_session.seed = generateSecuritySeed();
        m_session.securityLevel = subFunction / 2 + 1;

        UDSMessage msg;
        msg.serviceId = UDS_SID::SECURITY_ACCESS + UDS_SID::POSITIVE_RESPONSE_MASK;
        msg.subFunction = subFunction;
        quint32 seedLe = qToLittleEndian<quint32>(m_session.seed);
        msg.payload.append(reinterpret_cast<const char *>(&seedLe), 4);
        msg.isResponse = true;
        msg.timetemp = QDateTime::currentMSecsSinceEpoch();
        return msg;
    } else { // 提供密钥
        if (req.payload.size() < 4) {
            return makeNegativeResponse(UDS_SID::SECURITY_ACCESS,
                                        NegativeResponseCode::INCORRECT_MESSAGE_LENGTH);
        }

        uint32_t providedKey = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(req.payload.constData()));

        if (validateSecurityKey(m_session.seed, providedKey)) {
            m_session.isAuthenticated = true;
            UDSMessage msg;
            msg.serviceId = UDS_SID::SECURITY_ACCESS + UDS_SID::POSITIVE_RESPONSE_MASK;
            msg.subFunction = subFunction;
            msg.isResponse = true;
            msg.timetemp = QDateTime::currentMSecsSinceEpoch();
            return msg;
        } else {
            return makeNegativeResponse(UDS_SID::SECURITY_ACCESS,
                                        NegativeResponseCode::INVALID_KEY);
        }
    }
}

UDSMessage UDS_handler::handleECUReset(const UDSMessage &req)
{
    uint8_t subFunction = req.subFunction;
    if (subFunction != 0x01 && subFunction != 0x02 && subFunction != 0x03) {
        return makeNegativeResponse(UDS_SID::ECU_RESET,
                                    NegativeResponseCode::SUB_FUNCTION_NOT_SUPPORTED);
    }

    UDSMessage msg;
    msg.serviceId = UDS_SID::ECU_RESET + UDS_SID::POSITIVE_RESPONSE_MASK;
    msg.subFunction = subFunction;

    quint32 startupTimeMs = static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xFFFFFFFF);
    quint32 startupTimeLe = qToLittleEndian<quint32>(startupTimeMs);
    msg.payload.append(reinterpret_cast<const char *>(&startupTimeLe), 4);

    msg.isResponse = true;
    msg.timetemp = QDateTime::currentMSecsSinceEpoch();
    return msg;
}

uint32_t UDS_handler::generateSecuritySeed()
{
    auto now = QDateTime::currentMSecsSinceEpoch();
    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(QString::number(now).toUtf8());
    hash.addData(QByteArray::fromHex("DEADBEEF"));

    QByteArray hashResult = hash.result();
    uint32_t seed = qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(hashResult.constData()));

    if (seed == 0) seed = 0x12345678;

    return seed;
}

bool UDS_handler::validateSecurityKey(uint32_t seed, uint32_t key)
{
    uint32_t expectedKey = seed ^ 0xABCDEF00;
    return key == expectedKey;
}
