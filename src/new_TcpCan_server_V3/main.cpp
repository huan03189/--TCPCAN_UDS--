#include <QCoreApplication>
#include "server/mainserver.h"
#include "server/db_logger.h"
#include <QMetaType>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    qRegisterMetaType<quintptr>("quintptr");
    qRegisterMetaType<QByteArray>("QByteArray");
    qRegisterMetaType<int>("int");
    qRegisterMetaType<CanFrameData>("CanFrameData");
    qRegisterMetaType<CanFrameData>("const CanFrameData&");
    qRegisterMetaType<UDSMessage>("UDSMessage");
    qRegisterMetaType<UDSMessage>("const UDSMessage&");

    QString canIface = "can0";
    quint16 port = 8888;
    DB_logger::LogLevel logLevel = DB_logger::LEVEL_INFO;
    int canSampleRate = 1;
    bool skipTesterPresent = false;

    for (int i = 1; i < argc; i++) {
        QString arg = argv[i];
        if (arg == "-p" && i + 1 < argc) {
            port = QString(argv[++i]).toUShort();
        } else if (arg == "-c" && i + 1 < argc) {
            canIface = argv[++i];
        } else if (arg == "-q" || arg == "--quiet") {
            logLevel = DB_logger::LEVEL_WARNING;
        } else if (arg == "-v" || arg == "--verbose") {
            logLevel = DB_logger::LEVEL_DEBUG;
        } else if (arg == "--trace") {
            logLevel = DB_logger::LEVEL_TRACE;
        } else if (arg == "--can-sample" && i + 1 < argc) {
            canSampleRate = QString(argv[++i]).toInt();
        } else if (arg == "--skip-tester-present") {
            skipTesterPresent = true;
        } else if (arg == "--stress-mode") {
            // 压力测试模式：仅记录错误和警告
            logLevel = DB_logger::LEVEL_ERROR;
            skipTesterPresent = true;
            canSampleRate = 100;
        } else if (arg == "-h" || arg == "--help") {
            qInfo() << "Usage:" << argv[0] << "[-p port] [-c can_interface] [options]";
            qInfo() << "  -p <port>            TCP listen port (default: 8888)";
            qInfo() << "  -c <interface>       CAN interface name (default: can0)";
            qInfo() << "  -q, --quiet          仅记录 WARNING 及以上";
            qInfo() << "  -v, --verbose        记录 DEBUG 及以上";
            qInfo() << "  --trace              记录全部（含 CAN 帧、TesterPresent）";
            qInfo() << "  --can-sample <N>     CAN 帧每 N 帧记录 1 帧（默认 1）";
            qInfo() << "  --skip-tester-present 跳过 TesterPresent 事务日志";
            qInfo() << "  --stress-mode        压力测试模式（仅错误 + CAN 1/100 采样）";
            return 0;
        }
    }

    // 配置日志等级
    DB_logger::instance()->setLogLevel(logLevel);
    DB_logger::instance()->setCanFrameSampleRate(canSampleRate);
    DB_logger::instance()->setSkipTesterPresent(skipTesterPresent);

    qInfo() << "Starting server on port" << port << "with CAN interface" << canIface;
    qInfo() << "Log level:" << logLevel
            << " CAN sample: 1/" << canSampleRate
            << " Skip TesterPresent:" << (skipTesterPresent ? "ON" : "OFF");

    MainServer server;
    server.start(port, canIface);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        if (DB_logger::instance()) {
            DB_logger::instance()->flush();
        }
    });

    return app.exec();
}
