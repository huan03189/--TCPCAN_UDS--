#ifndef CLIENT_H
#define CLIENT_H

#include <QWidget>
#include <QThread>
#include "tcp_worker.h"
#include "can_worker.h"
#include "../stress_test_plot.h"
#include "stresstestmonitor.h"
namespace Ui {
class client;
}

class client : public QWidget
{
    Q_OBJECT

public:
    explicit client(QWidget *parent = nullptr);
    ~client();

    //安全访问相关函数
    void requestSecurityAccess();
    void sendSecurityKey(const QByteArray &key);
signals:
    void securityAccessRequired();

private slots:

    void on_btn_ReadTemp_clicked();

    void on_btn_ConnectTcp_clicked();

    void on_btn_ConnectCan_clicked();

    void onUdsMessageReceived(const UDSMessage&resp);

    void on_ex_btn_clicked();

    void on_Default_clicked();

    void on_code_btn_clicked();

    void on_btn_DisconnectTcp_clicked();

    void on_stress_btn_clicked();

    void on_btnMin_clicked();

    void on_btnClose_clicked();

    void on_btn_load_clicked();

    void on_btn_usage_clicked();

    // void on_btn_EcuReset_clicked();

    // void on_btn_TesterPresent_clicked();

    void on_btn_ECU_clicked();

    void on_btn_Tester_clicked();

private:
    Ui::client *ui;
    //线程指针
    QThread *m_tcpThread;
    QThread *m_canThread;
    //worker指针
    Tcp_worker *tcpWorker;
    Can_worker *canWorker;
    QString getNrcDescription(uint8_t nrc);
    QString getSessionTypeName(uint8_t sessionType);
    bool m_securityAccessCompleted=false;     //标记是否完成安全访问
    bool m_waitingForSecurityAccess = false;  // 标记是否正在等待安全访问完成
    UDSMessage m_pendingRequest;              // 存储等待中的请求
    enum PendingOperationType {
        NoOperation,
        EnterExtendedSession,
        EnterProgrammingSession,
        ReadData,
        OtherOperation
    };
    PendingOperationType m_pendingOperation = NoOperation;
    QByteArray calculateKeyFromSeed(const QByteArray &seed);
    void sendUDSRequest(const UDSMessage&request, const QString& description);
    void enterSession(uint8_t subFunction, const QString &logMsg, PendingOperationType pendingOp);

    //压力测试指针
    Stress_Test_Plot* m_test=nullptr;
    //压力测试线程
    QThread *m_monitorThread;
    StressTestMonitor *m_monitor;

    //UI
    bool m_dragging;
    QPoint m_dragPosition;
    enum SessionType {
        DefaultSession = 0x01,
        ProgrammingSession = 0x02,
        ExtendedSession = 0x03,
        UnknownSession = 0xFF
    };

    SessionType m_currentSession = DefaultSession;
protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // CLIENT_H
