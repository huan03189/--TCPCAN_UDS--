#include "client.h"
#include "ui_client.h"
#include"tcp_worker.h"
#include<QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include<QDebug>
#include<QTextCodec>
#include "core/udshelper.h"
#include <QInputDialog>
client::client(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::client)
    , m_test(nullptr)
    , m_monitorThread(nullptr)
    , m_monitor(nullptr)
{
    ui->setupUi(this);
    //去掉系统标题栏
    this->setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
    this->setAttribute(Qt::WA_TranslucentBackground);
    this->setStyleSheet("background-color:transparent; border-radius:6px;");
    ui->mainContainer->setStyleSheet(
        "background-color: #2C3E50;"
        "border-bottom-left-radius: 10px;"     // 左下角圆角
        "border-bottom-right-radius: 10px;"    // 右下角圆角
        );
    ui->Titlewidget->setStyleSheet(
        "background-color: #2C3E50;"
        "border-top-left-radius: 10px;"        // 左上角圆角
        "border-top-right-radius: 10px;"       // 右上角圆角
        );
    //拖动支持
    m_dragging = false;
    m_dragPosition = QPoint();
    ui->mainContainer->installEventFilter(this);

    //原来的控件初始化和布局属性
    ui->label_TcpStatus->setObjectName("label_TcpStatus");
    ui->label_CanStatus->setObjectName("label_CanStatus");

    //初始化线程
    m_tcpThread = new QThread(this);
    m_tcpThread->setObjectName("TCP Thread");
    tcpWorker = new Tcp_worker();
    tcpWorker->moveToThread(m_tcpThread);
    m_tcpThread->start();

    m_canThread = new QThread(this);
    m_canThread->setObjectName("CAN Thread");
    canWorker = new Can_worker();
    canWorker->moveToThread(m_canThread);
    m_canThread->start();
    //连接信号槽
    connect(canWorker, &Can_worker::udsMessageReceived, this, &client::onUdsMessageReceived,
            Qt::QueuedConnection);
    connect(tcpWorker,&Tcp_worker::udsMessageReceived,this,&client::onUdsMessageReceived,
            Qt::QueuedConnection);
    connect(tcpWorker,&Tcp_worker::connected,this,[this](){
        ui->label_TcpStatus->setProperty("tcpState", "ready");
        ui->label_TcpStatus->style()->unpolish(ui->label_TcpStatus);
        ui->label_TcpStatus->style()->polish(ui->label_TcpStatus);
        ui->label_TcpStatus->setText("TCP 就绪");
        ui->plainTextEdit_Log->appendPlainText(">>> TCP连接成功");
    },Qt::QueuedConnection);
    connect(tcpWorker,&Tcp_worker::reconnect,this,[this](){
        ui->label_TcpStatus->setProperty("tcpState", "reconnecting");
        ui->label_TcpStatus->style()->unpolish(ui->label_TcpStatus);
        ui->label_TcpStatus->style()->polish(ui->label_TcpStatus);
        ui->label_TcpStatus->setText("尝试重连...");
        ui->plainTextEdit_Log->appendPlainText(">>> TCP尝试重连");
    },Qt::QueuedConnection);
    connect(tcpWorker,&Tcp_worker::disconnected,this,[this](){
        ui->label_TcpStatus->setProperty("tcpState", "disconnected");
        ui->label_TcpStatus->style()->unpolish(ui->label_TcpStatus);
        ui->label_TcpStatus->style()->polish(ui->label_TcpStatus);
        ui->label_TcpStatus->setText("断开连接");
        ui->plainTextEdit_Log->appendPlainText(">>> TCP断开连接");
    },Qt::QueuedConnection);

    // 让温度居中
    ui->label_ResTemp->setAlignment(Qt::AlignCenter);

    // 让电压居中
    ui->label_Load->setAlignment(Qt::AlignCenter);

    // 让版本居中
    ui->label_Usage->setAlignment(Qt::AlignCenter);
}

client::~client()
{
    // 停止压力测试
    if (m_monitorThread && m_monitorThread->isRunning()) {
        QMetaObject::invokeMethod(m_monitor, "stop", Qt::BlockingQueuedConnection);
        m_monitorThread->quit();
        m_monitorThread->wait();
    }

    m_tcpThread->quit();
    m_canThread->quit();
    m_tcpThread->wait();
    m_canThread->wait();

    delete ui;
    if(m_test){
        m_test->close();
        delete m_test;
    }
}

void client::requestSecurityAccess(){
    //发送安全访问请求种子
    UDSMessage msg;
    msg.serviceId=0x27;
    msg.subFunction=0x01;
    msg.isResponse=false;

    QByteArray data = udshelper::serializeUDSMessage(msg);

    if(ui->comboBox_Channel->currentIndex()==1){
        QMetaObject::invokeMethod(tcpWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }else{
        QMetaObject::invokeMethod(canWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }
}

void client::sendSecurityKey(const QByteArray &key) {
    UDSMessage msg;
    msg.serviceId=0x27;
    msg.subFunction=0x02;
    msg.payload=key;
    msg.isResponse=false;
    QByteArray data = udshelper::serializeUDSMessage(msg);

    if(ui->comboBox_Channel->currentIndex()==1){
        QMetaObject::invokeMethod(tcpWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }else{
        QMetaObject::invokeMethod(canWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
    }
}



void client::sendUDSRequest(const UDSMessage&msg, const QString& description) {
    // 0x3E(测试者存在)和0x11(ECU复位)不需要安全访问
    bool skipAuth = (msg.serviceId == 0x3E || msg.serviceId == 0x11);
    if (!m_securityAccessCompleted && !skipAuth) {
        ui->plainTextEdit_Log->appendPlainText(
            QString("<<< %1 需要先完成安全访问...").arg(description));
        return;
    }
    QByteArray data = udshelper::serializeUDSMessage(msg);
    if(ui->comboBox_Channel->currentIndex()==1){
        QMetaObject::invokeMethod(tcpWorker,"sendUdsData",Qt::QueuedConnection,Q_ARG(QByteArray, data));

    }else{
        // QMetaObject::invokeMethod(canWorker,"sendUdsData",Qt::QueuedConnection,Q_ARG(QByteArray, data));
        // CAN 通道 - 检查是否初始化
        if (!Can_InterFace::Instance()) {
            qDebug() << "CAN not initialized, fallback to TCP";
            QMetaObject::invokeMethod(tcpWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        } else {
            QMetaObject::invokeMethod(canWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        }
    }
}


void client::on_btn_load_clicked()
{
    UDSMessage msg;

    msg.serviceId = 0x22;
    msg.payload.append(char(0x91));
    msg.payload.append(char(0xCC));

    sendUDSRequest(msg, "读取CPU负载");
}


void client::on_btn_usage_clicked()
{
    UDSMessage msg;

    msg.serviceId = 0x22;
    msg.payload.append(char(0x92));
    msg.payload.append(char(0xCC));

    sendUDSRequest(msg, "读取内存使用率");
}


void client::on_btn_ReadTemp_clicked()
{
    UDSMessage msg;

    msg.serviceId = 0x22;
    msg.payload.append(char(0x90));
    msg.payload.append(char(0xCC));

    sendUDSRequest(msg, "读取CPU温度");
}

void client::on_btn_ConnectTcp_clicked()
{
    QString ip = ui->lineEdit_Ip->text();
    int port = ui->lineEdit_Port->text().toUInt();


    QMetaObject::invokeMethod(tcpWorker, "initSocket", Qt::QueuedConnection);

    QMetaObject::invokeMethod(tcpWorker, "connectToServer",
                              Qt::QueuedConnection,
                              Q_ARG(QString, ip),
                              Q_ARG(quint16, port));

    ui->label_TcpStatus->setText("连接中...");
    ui->plainTextEdit_Log->appendPlainText(">>> TCP 尝试连接: " + ip + ":" + QString::number(port));

}

void client::on_btn_ConnectCan_clicked()
{
    QString darkStyle = R"(
    /* 对话框背景 */
    QDialog { background-color: #2C3E50; }
    /* 提示文字颜色 (如 "CAN 驱动名称:") */
    QLabel { color: #ECF0F1; font-size: 14px; }
    /* 输入框背景与文字 */
    QLineEdit {
        background-color: #34495E;
        color: #FFFFFF;
        border: 1px solid #5D6D7E;
        padding: 3px;
    }
    /* 按钮样式 (蓝色) */
    QPushButton {
        background-color: #3498DB;
        color: white;
        border: none;
        padding: 5px 15px;
        border-radius: 4px;
        min-width: 60px;
    }
    QPushButton:hover { background-color: #5DADE2; }
    QPushButton:pressed { background-color: #2980B9; }
)";

    // 临时应用样式到当前窗口
    this->setStyleSheet(darkStyle);
    bool ok;
    QString plugin = QInputDialog::getText(this, "CAN 初始化",
        "CAN 驱动名称:", QLineEdit::Normal, "peakcan", &ok);
    if (!ok || plugin.isEmpty()) return;

    QString iface = QInputDialog::getText(this, "CAN 初始化",
        "CAN 接口名称:", QLineEdit::Normal, "usb0", &ok);
    if (!ok || iface.isEmpty()) return;

    ui->label_CanStatus->setText("初始化中...");
    ui->label_CanStatus->setProperty("canState", "initializing");
    ui->label_CanStatus->style()->unpolish(ui->label_CanStatus);
    ui->label_CanStatus->style()->polish(ui->label_CanStatus);
    bool success = false;
    QMetaObject::invokeMethod(canWorker, "init",
                              Qt::BlockingQueuedConnection,
                              Q_RETURN_ARG(bool, success),
                              Q_ARG(QString, plugin),
                              Q_ARG(QString, iface));

    if (success) {
        ui->label_CanStatus->setText("CAN 就绪");
        ui->label_CanStatus->setProperty("canState", "ready");
        ui->label_CanStatus->style()->unpolish(ui->label_CanStatus);
        ui->label_CanStatus->style()->polish(ui->label_CanStatus);
        ui->plainTextEdit_Log->appendPlainText(">>> CAN 接口初始化成功");
    } else {
        ui->label_CanStatus->setText("CAN 失败");
        ui->label_CanStatus->setProperty("canState", "failed");
        ui->label_CanStatus->style()->unpolish(ui->label_CanStatus);
        ui->label_CanStatus->style()->polish(ui->label_CanStatus);
        ui->plainTextEdit_Log->appendPlainText("!!! CAN 接口初始化失败，请检查设备");
    }
}

QByteArray client::calculateKeyFromSeed(const QByteArray &seed) {
    if (seed.size() != 4) {
        // 如果不是4字节，返回原样
        return seed;
    }

    // 从种子字节数组构建32位整数（小端序，因为服务端用了qFromLittleEndian）
    uint32_t seedValue = 0;
    seedValue |= (static_cast<unsigned char>(seed[0]));      // LSB (最低有效字节)
    seedValue |= (static_cast<unsigned char>(seed[1]) << 8);
    seedValue |= (static_cast<unsigned char>(seed[2]) << 16);
    seedValue |= (static_cast<unsigned char>(seed[3]) << 24); // MSB (最高有效字节)

    // 应用服务端算法：种子 XOR 0xABCDEF00
    uint32_t keyValue = seedValue ^ 0xABCDEF00;

    // 将结果转换回字节数组（小端序）
    QByteArray key;
    key.append(keyValue & 0xFF);          // LSB
    key.append((keyValue >> 8) & 0xFF);
    key.append((keyValue >> 16) & 0xFF);
    key.append((keyValue >> 24) & 0xFF);  // MSB

    return key;
}

void client::onUdsMessageReceived(const UDSMessage&msg)
{
    if(msg.serviceId==0){
        return;
    }

    //否定响应
    if(msg.isNegativeResponse){
        ui->plainTextEdit_Log->appendPlainText(
            QString("!!! NRC: 0x%1")
                .arg(msg.negativeCode,2,16,QChar('0')));
        return;
    }

    //安全访问
    if(msg.serviceId == 0x67){
        if(msg.subFunction == 0x01){ // 收到种子
            QByteArray seed = msg.payload;
            QByteArray key = calculateKeyFromSeed(seed);
            sendSecurityKey(key);      // 自动发送密钥
            return;
        }

        if(msg.subFunction == 0x02){ // 安全访问完成
            ui->plainTextEdit_Log->appendPlainText("<<< 安全访问成功");
            m_securityAccessCompleted = true;
            m_waitingForSecurityAccess = false;

            // 自动触发待处理操作（拓展/编程会话）
            if(m_pendingOperation != NoOperation){
                switch(m_pendingOperation){
                case EnterExtendedSession:
                    enterSession(0x03, "进入拓展会话 (10 03)", EnterExtendedSession);
                    break;
                case EnterProgrammingSession:
                    enterSession(0x02, "进入编程会话 (10 02)", EnterProgrammingSession);
                    break;
                default:
                    break;
                }
                m_pendingOperation = NoOperation; // 清空
            }
            return;
        }
    }

    //DID读取
    if(msg.serviceId == 0x62){
        if(msg.payload.size() < 2) return;

        qDebug() << "[DID DEBUG] Received 0x62 response:"
                 << msg.payload.toHex()
                 << "isResponse:" << msg.isResponse
                 << "isNegative:" << msg.isNegativeResponse;

        uint16_t did = (uint8_t(msg.payload[0]) << 8) | uint8_t(msg.payload[1]);
        QByteArray actualData = msg.payload.mid(2);

        // --- 所有 UI 更新和 codec 转码都在 GUI 线程 ---
        QMetaObject::invokeMethod(this, [this, did, actualData](){
            switch(did){
            case 0x0320:{
                if(actualData.size() >= 1){
                    //int temp = uint8_t(actualData[0]);
                    //ui->label_ResTemp->setText(QString::number(temp) + " ℃");
                }
                break;
            }
            case 0x0420:{
                if(actualData.size() >= 4){
                    float voltage;
                    memcpy(&voltage, actualData.constData(), 4);
                    //ui->label_ResVolt->setText(QString::number(voltage,'f',2) + " V");
                }
                break;
            }
            case 0x90F1:{
                if(!actualData.isEmpty()){
                    QTextCodec *codec = QTextCodec::codecForName("GBK"); // GUI 线程安全
                    QString versionStr = codec->toUnicode(actualData);
                    //ui->label_ResVer->setText(versionStr);
                }
                break;
            }
            case 0x90CC:{  // CPU温度（4 字节 int LE）
                if(actualData.size() >= 4){
                    int32_t temp;
                    memcpy(&temp, actualData.constData(), 4);
                    ui->label_ResTemp->setText(QString::number(temp) + " ℃");
                    ui->plainTextEdit_Log->appendPlainText(
                        QString("<<< 飞腾派CPU温度: %1 ℃").arg(temp));
                }
                break;
            }
            case 0x91CC:{  // CPU负载（4 字节 float LE）
                if(actualData.size() >= 4){
                    float load;
                    memcpy(&load, actualData.constData(), 4);
                    ui->label_Load->setText(QString::number(load, 'f', 2));
                    ui->plainTextEdit_Log->appendPlainText(
                        QString("<<< 飞腾派CPU负载: %1").arg(load, 0, 'f', 2));
                }
                break;
            }
            case 0x92CC:{  // 内存使用率（4 字节 int LE）
                if(actualData.size() >= 4){
                    int32_t usage;
                    memcpy(&usage, actualData.constData(), 4);
                    ui->label_Usage->setText(QString::number(usage) + " %");
                    ui->plainTextEdit_Log->appendPlainText(
                        QString("<<< 飞腾派内存使用率: %1%").arg(usage));
                }
                break;
            }
            case 0x93CC:{  // 系统运行时间
                if(actualData.size() >= 4){
                    uint32_t uptime;
                    memcpy(&uptime, actualData.constData(), 4);
                    int hours = uptime / 3600;
                    int mins = (uptime % 3600) / 60;
                    ui->plainTextEdit_Log->appendPlainText(
                        QString("<<< 飞腾派运行时间: %1h %2min").arg(hours).arg(mins));
                }
                break;
            }
            case 0x94CC:{  // CAN状态
                if(actualData.size() >= 1){
                    QString statusStr;
                    switch(uint8_t(actualData[0])){
                    case 0x01: statusStr = "正常 (ERROR-ACTIVE)"; break;
                    case 0x02: statusStr = "错误被动 (ERROR-PASSIVE)"; break;
                    case 0x03: statusStr = "总线关闭 (BUS-OFF)"; break;
                    case 0x04: statusStr = "已停止 (STOPPED)"; break;
                    default: statusStr = "未知"; break;
                    }
                    ui->plainTextEdit_Log->appendPlainText(
                        QString("<<< CAN接口状态: %1").arg(statusStr));
                }
                break;
            }
            default:
                qDebug() << "[DID DEBUG] Unknown DID:" << QString::number(did,16);
                break;
            }
        }, Qt::QueuedConnection);
    }

    //会话控制
    if(msg.serviceId==0x50){
        ui->plainTextEdit_Log->appendPlainText("<<< 会话切换成功");
        return;
    }

    //ECU复位正响应
    if(msg.serviceId==0x51){
        ui->plainTextEdit_Log->appendPlainText("<<< ECU复位成功，控制器已重启");
        return;
    }

    //测试者存在正响应
    if(msg.serviceId==0x7E){
        // 压测时大量出现，不逐条打印日志
        return;
    }
}

QString client::getNrcDescription(uint8_t nrc) {
    switch(nrc) {
    case 0x10: return "通用拒绝";
    case 0x11: return "服务不支持";
    case 0x12: return "子功能不支持";
    case 0x22: return "条件不满足";
    case 0x31: return "请求超出范围";
    case 0x33: return "安全访问拒绝";
    case 0x78: return "请求正确响应待定";
    default: return QString("未知错误 (0x%1)").arg(nrc, 2, 16, QChar('0'));
    }
}

QString client::getSessionTypeName(uint8_t sessionType) {
    switch(sessionType) {
    case 0x01: return "默认会话";
    case 0x02: return "编程会话";
    case 0x03: return "扩展诊断会话";
    default: return QString("未知会话 (0x%1)").arg(sessionType, 2, 16, QChar('0'));
    }
}

void client::on_ex_btn_clicked()
{
    enterSession(0x03, "进入拓展会话 (10 03)", EnterExtendedSession);
}

void client::on_Default_clicked()
{
    enterSession(0x01, "进入默认会话 (10 01)", OtherOperation);
}

void client::on_code_btn_clicked()
{
    enterSession(0x02, "进入编程会话 (10 02)", EnterProgrammingSession);
}

void client::on_btn_DisconnectTcp_clicked()
{
    QMetaObject::invokeMethod(tcpWorker, "DisconnectToServer", Qt::QueuedConnection);

    ui->label_TcpStatus->setText("断开连接");
    ui->plainTextEdit_Log->appendPlainText(">>> TCP断开连接");
    ui->label_TcpStatus->setText("未连接");
}

void client::on_stress_btn_clicked()
{
    qDebug() << "[压力测试] ========== 按钮点击 ==========";

    //停止上一次测试
    if (m_monitorThread && m_monitorThread->isRunning()) {
        qDebug() << "[压力测试] 正在停止上一次测试...";

        if (m_monitor && m_monitor->thread() == m_monitorThread) {
            QMetaObject::invokeMethod(m_monitor, "stop", Qt::QueuedConnection);
        }

        QThread::msleep(100);

        m_monitorThread->quit();

        if (!m_monitorThread->wait(3000)) {
            m_monitorThread->terminate();
            m_monitorThread->wait();
        }

        if (m_monitor) {
            delete m_monitor;
            m_monitor = nullptr;
        }
        delete m_monitorThread;
        m_monitorThread = nullptr;

        qDebug() << "[压力测试] 上一次测试已停止";
    }

    //删除旧窗口
    if (m_test) {
        m_test->deleteLater();
        m_test = nullptr;
    }

    //检查 canWorker
    if (!canWorker) {
        qDebug() << "[压力测试] canWorker 为空!";
        return;
    }

    //创建新窗口
    m_test = new Stress_Test_Plot();
    m_test->setAttribute(Qt::WA_DeleteOnClose, false);
    m_test->setWindowTitle("CAN 总线负载压力测试");

    //连接关闭信号
    connect(m_test, &Stress_Test_Plot::WindowsClosed, this, [this](){
        qDebug() << "[压力测试] 窗口关闭按钮被点击";

        if (m_monitorThread && m_monitorThread->isRunning()) {
            QMetaObject::invokeMethod(m_monitor, "stop", Qt::QueuedConnection);
            QThread::msleep(100);
            m_monitorThread->quit();
            m_monitorThread->wait(3000);

            if (m_monitor) {
                delete m_monitor;
                m_monitor = nullptr;
            }
            delete m_monitorThread;
            m_monitorThread = nullptr;
        }
    });

    m_test->show();

    //创建线程和监控器
    m_monitorThread = new QThread(this);
    m_monitorThread->setObjectName("StressMonitor Thread");

    m_monitor = new StressTestMonitor();
    m_monitor->moveToThread(m_monitorThread);

    connect(canWorker, &Can_worker::signalFrameSent,
            m_monitor, &StressTestMonitor::onFrameSent);
    connect(canWorker, &Can_worker::signalFrameReceived,
            m_monitor, &StressTestMonitor::onFrameReceived);
    connect(canWorker, &Can_worker::udsRequestSentSignal,
            m_monitor, &StressTestMonitor::onRequestSent);
    connect(canWorker, &Can_worker::udsResponseReceivedSignal,
            m_monitor, &StressTestMonitor::onResponseReceived);

    connect(m_monitor, &StressTestMonitor::statsUpdated,
            m_test, &Stress_Test_Plot::updateChartData);
    connect(m_monitor, &StressTestMonitor::requestToSendSignal,
            canWorker, &Can_worker::sendUdsMessage,
            Qt::QueuedConnection);
    connect(m_monitor, &StressTestMonitor::testFinished,
            this, [this](quint64 totalBits, quint32 sentCount, quint32 receivedCount) {
                // 可选：弹窗提示
                QMessageBox::information(nullptr, "压力测试完成",
                                         QString("5分钟压力测试完成!\n\n"
                                                 "发送请求: %1\n"
                                                 "收到响应: %2\n"
                                                 "丢包率: %3%")
                                             .arg(sentCount)
                                             .arg(receivedCount)
                                             .arg((1.0 - (double)receivedCount/sentCount) * 100, 0, 'f', 2));
            }, Qt::QueuedConnection);
    m_monitorThread->start();
    QString darkStyle = R"(
    /* 对话框背景 */
    QDialog { background-color: #2C3E50; }
    /* 提示文字颜色 (如 "CAN 驱动名称:") */
    QLabel { color: #ECF0F1; font-size: 14px; }
    /* 输入框背景与文字 */
    QLineEdit {
        background-color: #34495E;
        color: #FFFFFF;
        border: 1px solid #5D6D7E;
        padding: 3px;
    }
    /* 按钮样式 (蓝色) */
    QPushButton {
        background-color: #3498DB;
        color: white;
        border: none;
        padding: 5px 15px;
        border-radius: 4px;
        min-width: 60px;
    }
    QPushButton:hover { background-color: #5DADE2; }
    QPushButton:pressed { background-color: #2980B9; }
)";

    //临时应用样式到当前窗口
    this->setStyleSheet(darkStyle);
    bool ok;
    int startFreq = QInputDialog::getInt(this, "压力测试参数",
        "起始频率 (req/s):", 20, 1, 10000, 10, &ok);
    if (!ok) return;
    int maxFreq = QInputDialog::getInt(this, "压力测试参数",
        "最大频率 (req/s):", 300, startFreq, 10000, 10, &ok);
    if (!ok) return;
    int stepFreq = QInputDialog::getInt(this, "压力测试参数",
        "步进量 (req/s):", 10, 1, 1000, 5, &ok);
    if (!ok) return;
    int stepMs = QInputDialog::getInt(this, "压力测试参数",
        "加压间隔 (ms):", 2000, 500, 60000, 500, &ok);
    if (!ok) return;

    QMetaObject::invokeMethod(m_monitor, [this, startFreq, maxFreq, stepFreq, stepMs]() {
        m_monitor->setCanBitrate(500000);
        m_monitor->setStressParameters(startFreq, maxFreq, stepFreq, stepMs);
        m_monitor->start();
    }, Qt::QueuedConnection);

    qDebug() << "[压力测试] 基础版本启动完成";
}

bool client::eventFilter(QObject *obj, QEvent *event)
{
    if(obj==ui->mainContainer)
    {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        switch(event->type())
        {
        case QEvent::MouseButtonPress:
            if(mouseEvent->button() == Qt::LeftButton){
                m_dragging = true;
                m_dragPosition = mouseEvent->globalPosition().toPoint() - frameGeometry().topLeft();
                return true;
            }
            break;
        case QEvent::MouseMove:
            if(m_dragging){
                move(mouseEvent->globalPosition().toPoint() - m_dragPosition);
                return true;
            }
            break;
        case QEvent::MouseButtonRelease:
            if(mouseEvent->button() == Qt::LeftButton){
                m_dragging = false;
                return true;
            }
            break;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void client::on_btnMin_clicked()
{
    this->showMinimized();
}


void client::on_btnClose_clicked()
{
    this->close();
}

void client::enterSession(uint8_t subFunction, const QString &logMsg, PendingOperationType pendingOp){
    ui->plainTextEdit_Log->appendPlainText(QString("<<< %1按钮点击 - 当前安全访问状态: %2, 待处理操作: %3")
                                               .arg(logMsg)
                                               .arg(m_securityAccessCompleted ? "已完成" : "未完成")
                                               .arg(
                                                   m_pendingOperation == EnterExtendedSession ? "进入拓展会话" :
                                                       m_pendingOperation == EnterProgrammingSession ? "进入编程会话" :
                                                       m_pendingOperation == ReadData ? "读数据" :
                                                       m_pendingOperation == OtherOperation ? "其他" : "无"));

    // 安全访问判断
    bool needSecurityAccess = ( (subFunction == 0x03 || subFunction == 0x02) && !m_securityAccessCompleted );

    if (needSecurityAccess) {
        ui->plainTextEdit_Log->appendPlainText(QString("<<< %1需要先完成安全访问...").arg(logMsg));
        m_pendingOperation = pendingOp;
        m_waitingForSecurityAccess = true;
        requestSecurityAccess();
        return;
    }

    //构建 UDS 请求
    UDSMessage msg;
    msg.serviceId = UDS_SID::DIAGNOSTIC_SESSION_CONTROL; // 0x10
    msg.subFunction = subFunction;
    msg.isResponse = false;

    QByteArray data = udshelper::serializeUDSMessage(msg);

    // 发送
    if (ui->comboBox_Channel->currentIndex() == 1) {
        QMetaObject::invokeMethod(tcpWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        ui->plainTextEdit_Log->appendPlainText(QString(">>> TCP发送 UDS: %1").arg(logMsg));
    } else {
        // QMetaObject::invokeMethod(canWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        // ui->plainTextEdit_Log->appendPlainText(QString(">>> CAN发送 UDS: %1").arg(logMsg));
        // CAN 通道 - 检查是否初始化
        if (!Can_InterFace::Instance()) {
            qDebug() << "CAN not initialized, fallback to TCP";
            QMetaObject::invokeMethod(tcpWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        } else {
            QMetaObject::invokeMethod(canWorker, "sendUdsData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
        }
    }

    //更新当前会话状态
    switch(subFunction) {
    case 0x01: m_currentSession = DefaultSession; break;
    case 0x02: m_currentSession = ProgrammingSession; break;
    case 0x03: m_currentSession = ExtendedSession; break;
    default: m_currentSession = UnknownSession; break;
    }

    //切回默认会话时清理安全访问标志
    if(subFunction == 0x01) {
        m_securityAccessCompleted = false;
        m_waitingForSecurityAccess = false;
    }

    //清空待处理操作
    m_pendingOperation = NoOperation;
}




void client::on_btn_ECU_clicked()
{
    UDSMessage msg;
    msg.serviceId = 0x11;
    msg.subFunction = 0x01;  //硬件复位
    sendUDSRequest(msg, "ECU 硬件复位");
}


void client::on_btn_Tester_clicked()
{
        UDSMessage msg;
        msg.serviceId = 0x3E;
        msg.subFunction = 0x00;
        sendUDSRequest(msg, "测试者存在");

        //将回复显示在日志中
        ui->plainTextEdit_Log->appendPlainText(
            QString(">>> 发送测试者存在 (0x3E)"));
}

