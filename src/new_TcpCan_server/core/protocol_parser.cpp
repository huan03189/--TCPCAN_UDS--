#include "protocol_parser.h"
#include "CRC_helper.h"
#include "server/db_logger.h"
#include <QDebug>

protocol_parser::protocol_parser(QObject *parent)
    : QObject{parent}
{}

void protocol_parser::pushData(const QByteArray &data)
{
    m_buffer.append(data);

    while (static_cast<size_t>(m_buffer.size()) >= TCP_HEADER_SIZE) {
        const uchar *ptr = reinterpret_cast<const uchar *>(m_buffer.data());

        // 魔数校验
        if (ptr[0] != 0xCD || ptr[1] != 0xAB) {
            DB_logger::instance()->log(DB_logger::LOG_DEBUG,
                                       "Protocol: bad magic byte, resync",
                                       QString("Byte: 0x%1").arg(ptr[0], 2, 16, QChar('0')));
            m_buffer.remove(0, 1);
            continue;
        }

        uint8_t type = ptr[2];
        uint16_t length = (ptr[4] << 8) | ptr[3];

        if (length > 1024) {
            DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                       "Protocol: oversized payload, clearing buffer",
                                       QString("Length: %1").arg(length));
            m_buffer.clear();
            return;
        }

        int totalLen = 5 + length + 2;

        if (m_buffer.size() < totalLen) {
            break;
        }

        QByteArray payload = m_buffer.mid(5, length);

        // CRC 校验
        int crcOffset = totalLen - 2;
        uint16_t receivedCRC = (ptr[crcOffset + 1] << 8) | ptr[crcOffset];
        uint16_t calculatedCRC = CRC_helper::calculate(payload);

        if (receivedCRC != calculatedCRC) {
            DB_logger::instance()->log(DB_logger::LOG_CRC_ERROR,
                                       "CRC mismatch, dropping 1 byte to resync",
                                       QString("Received: 0x%1  Calculated: 0x%2  PayloadLen: %3")
                                           .arg(receivedCRC, 4, 16, QChar('0'))
                                           .arg(calculatedCRC, 4, 16, QChar('0'))
                                           .arg(length));
            m_buffer.remove(0, 1);
            continue;
        }

        switch (type) {
        case 1: {
            if (length < 4) {
                DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                           "Protocol: invalid CAN packet length",
                                           QString::number(length));
                break;
            }

            CanFrameData frame;
            frame.id = static_cast<uint32_t>(ptr[5]
                                             | (ptr[6] << 8)
                                             | (ptr[7] << 16)
                                             | (ptr[8] << 24));
            frame.len = length - 4;

            if (frame.len > 8) {
                DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                           "Protocol: CAN DLC overflow",
                                           QString("DLC: %1").arg(frame.len));
                break;
            }

            memcpy(frame.data, ptr + 9, frame.len);
            emit canFrameReady(frame);
            break;
        }
        case 2: {
            break; // 心跳
        }
        case 3: {
            if (payload.isEmpty()) break;
            emit udsPacketReady(payload);
            break;
        }
        default: {
            DB_logger::instance()->log(DB_logger::LOG_WARNING,
                                       "Protocol: unknown packet type",
                                       QString::number(type));
            break;
        }
        }

        m_buffer.remove(0, totalLen);
    }
}
