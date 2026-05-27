#ifndef CRC_HELPER_H
#define CRC_HELPER_H
#include <cstdint>
#include <QByteArray>

class CRC_helper{
public:
    //计算CRC16 Modbus
    static uint16_t calculate(const uint8_t *data, int length) {
        uint16_t crc = 0xFFFF;
        for (int i = 0; i < length; i++) {
            crc ^= data[i];
            for (int j = 0; j < 8; j++) {
                if (crc & 0x0001) {
                    crc >>= 1;
                    crc ^= 0xA001; // 多项式
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
    static uint16_t calculate(const QByteArray &data) {
        return calculate(reinterpret_cast<const uint8_t*>(data.constData()), data.size());
    }
};

#endif // CRC_HELPER_H
