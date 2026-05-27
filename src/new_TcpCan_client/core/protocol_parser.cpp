#include "protocol_parser.h"
#include "CRC_helper.h"
#include <QDebug>
#include "udshelper.h"
#include <QtEndian>
protocol_parser::protocol_parser(QObject *parent)
    : QObject{parent}
{}

//数据添加到缓存区
void protocol_parser::pushData(const QByteArray &data){
    m_buffer.append(data);

    // 循环处理包，防止一次收到多个包的情况
    while (m_buffer.size() >= 7)
    {
        //检查魔数
        uint16_t magic = qFromLittleEndian<uint16_t>(reinterpret_cast<const uchar*>(m_buffer.data()));

        if (magic != 0xABCD) {
            qDebug() << "[Parser] Magic error, dropping 1 byte to re-sync";
            m_buffer.remove(0, 1);
            continue;
        }

        //读取包头
        uint8_t type=static_cast<uint8_t>(m_buffer.at(2));
        uint16_t length =
            qFromLittleEndian<uint16_t>(
                reinterpret_cast<const uchar*>(m_buffer.data() + 3));

        //计算包长度
        int totalLen=5+length+2;
        if(m_buffer.size()<totalLen){
            break;
        }

        //提取Payload
        QByteArray payload=m_buffer.mid(5,length);

        //CRC校验
        int crcOffset=5+length;
        uint16_t receivedCRC =
            qFromLittleEndian<uint16_t>(
                reinterpret_cast<const uchar*>(
                    m_buffer.data() + crcOffset));
        uint16_t calculatedCRC =
            CRC_helper::calculate(payload);
        if (receivedCRC != calculatedCRC)
        {
            qDebug() << "[Parser] CRC Error!";
            qDebug() << "[Parser] Received CRC:"
                     << QString::number(receivedCRC, 16).toUpper();

            qDebug() << "[Parser] Calculated CRC:"
                     << QString::number(calculatedCRC, 16).toUpper();

            qDebug() << "[Parser] Payload:"
                     << payload.toHex();

            emit parseError(
                QString("CRC Error! Type=%1").arg(type));

            // 丢弃当前包
            m_buffer.remove(0, totalLen);

            continue;
        }


        //Type分发
        if (type == 1)
        {
            if (length >= 4)
            {
                CanFrameData frame;

                frame.id =
                    qFromLittleEndian<uint32_t>(
                        reinterpret_cast<const uchar*>(payload.data()));

                frame.len =
                    length - 4;

                if (frame.len <= 8)
                {
                    memcpy(frame.data,
                           payload.data() + 4,
                           frame.len);

                    emit canFrameReady(frame);
                }
            }
        }
        // Heartbeat
        else if (type == 2)
        {
            // Heartbeat - silently handled
        }
        // UDS Message
        else if (type == 3)
        {
            QByteArray udsPayload = payload;

            // 统一反序列化
            UDSMessage msg =udshelper::deserializeUDSMessage(udsPayload);

            emit udsMessageReady(msg);
        }else{
            qDebug()<<"[Parser] Unknown Packet Type:"<< type;
        }

        //移除已处理数据
        m_buffer.remove(0,totalLen);
    }
}

