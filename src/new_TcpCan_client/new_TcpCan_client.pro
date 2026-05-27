QT       += core gui network serialbus sql core5compat charts

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    client/can_worker.cpp \
    client/client.cpp \
    client/stresstestmonitor.cpp \
    client/tcp_worker.cpp \
    core/can_interface.cpp \
    core/protocol_parser.cpp \
    core/udshelper.cpp \
    isotp/isotp.c \
    isotp/isotp_user_impl.c \
    main.cpp \
    stress_test_plot.cpp

HEADERS += \
    client/can_worker.h \
    client/client.h \
    client/stresstestmonitor.h \
    client/tcp_worker.h \
    core/CRC_helper.h \
    core/can_interface.h \
    core/protocol_parser.h \
    core/ring_buffer.h \
    core/types.h \
    core/udshelper.h \
    isotp/isotp.h \
    isotp/isotp_config.h \
    isotp/isotp_defines.h \
    isotp/isotp_user.h \
    stress_test_plot.h

FORMS += \
    client/client.ui
# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
