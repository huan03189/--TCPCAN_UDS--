QT = core sql network

CONFIG += c++17 cmdline

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    core/can_interface.cpp \
    core/did_database.cpp \
    core/protocol_parser.cpp \
    core/uds_handler.cpp \
    core/uds_worker.cpp \
    core/udshelper.cpp \
    isotp/isotp.c \
    isotp/isotp_user_impl.c \
    main.cpp \
    server/can_worker.cpp \
    server/can_worker_server.cpp \
    server/db_logger.cpp \
    server/mainserver.cpp \
    server/tcpworkerserver.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    core/CRC_helper.h \
    core/can_interface.h \
    core/did_database.h \
    core/protocol_parser.h \
    core/ring_buffer.h \
    core/types.h \
    core/uds_handler.h \
    core/uds_worker.h \
    core/udshelper.h \
    isotp/isotp.h \
    isotp/isotp_config.h \
    isotp/isotp_defines.h \
    isotp/isotp_user.h \
    server/can_worker.h \
    server/can_worker_server.h \
    server/db_logger.h \
    server/mainserver.h \
    server/tcpworkerserver.h
