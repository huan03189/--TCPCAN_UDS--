#ifndef UDS_HANDLER_H
#define UDS_HANDLER_H

#include <QObject>
#include <QByteArray>
#include "types.h"
#include "did_database.h"
class UDS_handler : public QObject
{
    Q_OBJECT
public:
    explicit UDS_handler(QObject *parent = nullptr);

    //处理原始UDS请求字节流
    UDSMessage processRequest(const UDSMessage& req);

    // // 获取当前会话状态（供界面显示）
    uint8_t getCurrentSession() const { return m_session.currentSession; }
    bool isSecurityAccessGranted() const { return m_session.isAuthenticated; }


private:
    struct SessionState{
        uint8_t currentSession = DiagnosticSession::DEFAULT_SESSION;
        bool isAuthenticated = false;
        uint8_t securityLevel = 0;
        uint32_t seed = 0;
    }m_session;

    DID_database* m_didDatabase;

    //各服务处理函数
    UDSMessage handleDiagnosticSessionControl(const UDSMessage& req);
    UDSMessage handleReadDataByIdentifier(const UDSMessage& req);
    UDSMessage handleTesterPresent(const UDSMessage& req);
    UDSMessage handleSecurityAccess(const UDSMessage& req);
    UDSMessage handleECUReset(const UDSMessage& req);
    //辅助函数
    UDSMessage makePositiveResponse(uint8_t sid, const QByteArray& payload = {});
    UDSMessage makeNegativeResponse(uint8_t sid, uint8_t nrc);


    QByteArray convertVariantToBytes(const QVariant& value);
    uint32_t generateSecuritySeed();
    bool validateSecurityKey(uint32_t seed, uint32_t key);
};

#endif // UDS_HANDLER_H
