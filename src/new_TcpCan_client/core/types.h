#ifndef TYPES_H
#define TYPES_H
#include<cstdint>
#include<QString>
#include <QMetaType>

//CAN ID 定义
namespace CanId{
    constexpr uint32_t ID_BODY_CTRL=0x100;//车身控制
    constexpr uint32_t ID_NAV_INFO=0x200;//导航信息
    constexpr uint32_t ID_UDS_REQ=0x7E1;//UDS诊断请求
    constexpr uint32_t ID_UDS_RESP=0x7E9;//UDS诊断响应
    constexpr uint32_t ID_UDS_FUNC=0x7DF;//UDS功能寻址
    };

//0x100(车身控制命令)
namespace BodyCmd {
    constexpr uint8_t CMD_VOL_UP=0x01;
    constexpr uint8_t CMD_VOL_DOWN=0x02;
    constexpr uint8_t CMD_VOL_MUTE=0x03;
    constexpr uint8_t CMD_POWER_ON=0x04;
    constexpr uint8_t CMD_POWER_OFF=0x05;
}

//UDS服务ID定义（依据ISO 14229-1）
namespace UDS_SID {
    constexpr uint8_t DIAGNOSTIC_SESSION_CONTROL = 0x10;  // 诊断会话控制
    constexpr uint8_t ECU_RESET = 0x11;                  // ECU重置
    constexpr uint8_t SECURITY_ACCESS = 0x27;            // 安全访问
    constexpr uint8_t COMMUNICATION_CONTROL = 0x28;      // 通信控制
    constexpr uint8_t READ_DATA_BY_IDENTIFIER = 0x22;    // 读数据标识符
    constexpr uint8_t WRITE_DATA_BY_IDENTIFIER = 0x2E;   // 写数据标识符
    constexpr uint8_t TESTER_PRESENT = 0x3E;             // 测试者存在
    constexpr uint8_t ROUTINE_CONTROL = 0x31;            // 例行程序控制
    constexpr uint8_t NEGATIVE_RESPONSE = 0x7F;          // 负响应
    constexpr uint8_t POSITIVE_RESPONSE_MASK = 0x40;     // 正响应掩码
}

//诊断会话类型
namespace DiagnosticSession{
    constexpr uint8_t DEFAULT_SESSION = 0x01;
    constexpr uint8_t PROGRAMMING_SESSION = 0x02;
    constexpr uint8_t EXTENDED_DIAGNOSTIC = 0x03;
    constexpr uint8_t SAFETY_SYSTEM_DIAGNOSTIC = 0x04;
}

//负响应码（NRC）
namespace NegativeResponseCode {
    constexpr uint8_t SERVICE_NOT_SUPPORTED = 0x11;
    constexpr uint8_t SUB_FUNCTION_NOT_SUPPORTED = 0x12;
    constexpr uint8_t INCORRECT_MESSAGE_LENGTH = 0x13;
    constexpr uint8_t RESPONSE_TOO_LONG = 0x14;
    constexpr uint8_t BUSY_REPEAT_REQUEST = 0x21;
    constexpr uint8_t CONDITIONS_NOT_CORRECT = 0x22;
    constexpr uint8_t REQUEST_SEQUENCE_ERROR = 0x24;
    constexpr uint8_t NO_RESPONSE_FROM_SUBNET_COMPONENT = 0x25;
    constexpr uint8_t FAILURE_PREVENTS_EXECUTION_OF_REQUESTED_ACTION = 0x26;
    constexpr uint8_t REQUEST_OUT_OF_RANGE = 0x31;
    constexpr uint8_t SECURITY_ACCESS_DENIED = 0x33;
    constexpr uint8_t INVALID_KEY = 0x35;
    constexpr uint8_t EXCEED_NUMBER_OF_ATTEMPTS = 0x36;
    constexpr uint8_t REQUIRED_TIME_DELAY_NOT_EXPIRED = 0x37;
    constexpr uint8_t UPLOAD_DOWNLOAD_NOT_ACCEPTED = 0x70;
    constexpr uint8_t TRANSFER_DATA_SUSPENDED = 0x71;
    constexpr uint8_t GENERAL_PROGRAMMING_FAILURE = 0x72;
    constexpr uint8_t WRONG_BLOCK_SEQUENCE_COUNTER = 0x73;
    constexpr uint8_t REQUEST_CORRECTLY_RECEIVED_RESPONSE_PENDING = 0x78;
    constexpr uint8_t SUB_FUNCTION_NOT_SUPPORTED_IN_ACTIVE_SESSION = 0x7E;
    constexpr uint8_t SERVICE_NOT_SUPPORTED_IN_ACTIVE_SESSION = 0x7F;
    constexpr uint8_t RPM_TOO_HIGH = 0x81;
    constexpr uint8_t RPM_TOO_LOW = 0x82;
    constexpr uint8_t ENGINE_IS_RUNNING = 0x83;
    constexpr uint8_t ENGINE_IS_NOT_RUNNING = 0x84;
    constexpr uint8_t ENGINE_RUN_TIME_TOO_SHORT = 0x85;
    constexpr uint8_t TEMPERATURE_TOO_HIGH = 0x86;
    constexpr uint8_t TEMPERATURE_TOO_LOW = 0x87;
    constexpr uint8_t VEHICLE_SPEED_TOO_HIGH = 0x88;
    constexpr uint8_t VEHICLE_SPEED_TOO_LOW = 0x89;
    constexpr uint8_t THROTTLE_PEDAL_TOO_HIGH = 0x8A;
    constexpr uint8_t THROTTLE_PEDAL_TOO_LOW = 0x8B;
    constexpr uint8_t TRANSMISSION_RANGE_NOT_IN_NEUTRAL = 0x8C;
    constexpr uint8_t TRANSMISSION_RANGE_NOT_IN_GEAR = 0x8D;
    constexpr uint8_t BRAKE_SWITCH_NOT_CLOSED = 0x8F;
    constexpr uint8_t SHIFTER_LEVER_NOT_IN_PARK = 0x90;
    constexpr uint8_t TORQUE_CONVERTER_CLUTCH_LOCKED = 0x91;
    constexpr uint8_t VOLTAGE_TOO_HIGH = 0x92;
    constexpr uint8_t VOLTAGE_TOO_LOW = 0x93;
}

struct UDSMessage{
    uint32_t requestId;
    uint8_t serviceId;
    uint8_t subFunction;
    QByteArray payload;
    qint64 timetemp;
    bool isResponse;
    bool isNegativeResponse;
    uint8_t negativeCode;
};
Q_DECLARE_METATYPE(UDSMessage)


//数据标识符（DID）定义
namespace DataIdentifier {
    // 车辆信息相关
    constexpr uint16_t VEHICLE_MANUFACTURER_SPARE_PART_NUMBER = 0xF187;
    constexpr uint16_t VEHICLE_MANUFACTURER_ECU_SOFTWARE_VERSION_NUMBER = 0xF189;
    constexpr uint16_t APPLICATION_SOFTWARE_IDENTIFICATION = 0xF190;
    constexpr uint16_t BOOT_SOFTWARE_FINGERPRINT = 0xF194;

    // 诊断相关信息
    constexpr uint16_t OBD_EMISSIONS_REGULATIONS_COMPLIANCE = 0xF1A0;
    constexpr uint16_t SYSTEM_SUPPLIER_IDENTIFIER = 0xF191;
    constexpr uint16_t ECUserialNumber = 0xF1A2;
    constexpr uint16_t SUPPORTED_DIAGNOSTIC_SERVICES = 0xF1A3;

    // 车身控制相关
    constexpr uint16_t CURRENT_VOLUME_LEVEL = 0x2001;
    constexpr uint16_t MAX_VOLUME_LEVEL = 0x2002;
    constexpr uint16_t CURRENT_TEMPERATURE = 0x2003;
    constexpr uint16_t BATTERY_VOLTAGE = 0x2004;
    constexpr uint16_t ENGINE_RPM = 0x2005;
    constexpr uint16_t VEHICLE_SPEED = 0x2006;
}

//物理层帧结构
struct CanFrameData{
    uint32_t id;
    uint8_t data[8];
    uint8_t len;
};

//TCP 协议头
#pragma pack(push,1)
struct TcpHeader{
    uint16_t magic;//0xABCD
    uint8_t type;//1,can数据 2,心跳 3,UDS
    uint16_t length;
};
#pragma pack(pop)

// TCP 帧头长度 (magic 2B + type 1B + length 2B = 5B)
constexpr int TCP_HEADER_SIZE = 5;

#endif // TYPES_H
