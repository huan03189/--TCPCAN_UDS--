#!/bin/bash
# 飞腾派 CAN 接口初始化 + 诊断脚本
# 每次启动前以 root 运行: sudo bash setup_can_irq.sh [can0] [1]

INTERFACE=${1:-can0}
TARGET_CORE=${2:-1}
BITRATE=${3:-500000}

echo "=== CAN Setup & Diagnostics ==="
echo "Interface: $INTERFACE"
echo "Target Core: CPU$TARGET_CORE"
echo "Bitrate: $BITRATE bps"
echo ""

# 1. 检查 CAN 接口是否存在
if ! ip link show "$INTERFACE" &>/dev/null; then
    echo "ERROR: $INTERFACE not found in system"
    echo "Available network interfaces:"
    ip link show | grep -E "^\d+:" | awk '{print "  "$2}'
    exit 1
fi

# 2. 检查收发器使能 GPIO（常见引脚）
echo "=== Checking CAN Transceiver GPIO ==="
for gpio in 24 25 26 27 120 121 133 134; do
    if [ -d "/sys/class/gpio/gpio$gpio" ]; then
        VAL=$(cat /sys/class/gpio/gpio$gpio/value 2>/dev/null)
        DIR=$(cat /sys/class/gpio/gpio$gpio/direction 2>/dev/null)
        LABEL=$(cat /sys/class/gpio/gpio$gpio/label 2>/dev/null)
        echo "  GPIO$gpio: dir=$DIR value=$VAL label=$LABEL"
    fi
done 2>/dev/null
echo ""

# 3. 关闭接口，重新配置（含 BUS-OFF 自动恢复）
echo "=== Configuring $INTERFACE ==="
ip link set "$INTERFACE" down 2>/dev/null

# restart-ms 100: BUS-OFF 后自动等 100ms 复位
ip link set "$INTERFACE" type can bitrate "$BITRATE" restart-ms 100

# 飞腾平台 CAN 控制器要求 MTU=16（CAN FD 兼容）
ip link set "$INTERFACE" mtu 16

# 开启接口
ip link set "$INTERFACE" up

# 4. 检查接口状态
echo "=== Interface Status ==="
ip -details link show "$INTERFACE" | grep -E "can|state|bitrate|restart|ERROR|BUS"

# 提取 CAN 状态关键字
CAN_STATE=$(ip -details link show "$INTERFACE" | grep -oE "ERROR-ACTIVE|ERROR-PASSIVE|BUS-OFF|STOPPED")
if [ "$CAN_STATE" = "ERROR-ACTIVE" ]; then
    echo "Status: OK ($CAN_STATE)"
elif [ "$CAN_STATE" = "BUS-OFF" ]; then
    echo "Status: FAILED! ($CAN_STATE)"
    echo ""
    echo "BUS-OFF means the CAN controller cannot communicate."
    echo "Check the following:"
    echo "  1. Is the CAN transceiver chip powered? (check VCC pin)"
    echo "  2. Is the transceiver enable GPIO set correctly?"
    echo "  3. Are termination resistors (120 Ohm) installed at both ends?"
    echo "  4. Is the other CAN node connected and powered?"
    echo "  5. Do both nodes use the same bitrate ($BITRATE)?"
elif [ -z "$CAN_STATE" ]; then
    echo "WARNING: Cannot determine CAN state. Is the interface up?"
else
    echo "Status: $CAN_STATE (errors present, check wiring)"
fi
echo ""

# 5. 查找并绑定 CAN 中断
echo "=== IRQ Affinity ==="
IRQ=$(grep -i "$INTERFACE" /proc/interrupts | head -1 | awk -F: '{print $1}' | tr -d ' ')

if [ -n "$IRQ" ]; then
    MASK=$((1 << TARGET_CORE))
    echo "$MASK" > /proc/irq/$IRQ/smp_affinity 2>/dev/null
    ACTUAL=$(cat /proc/irq/$IRQ/smp_affinity 2>/dev/null || echo "N/A")
    echo "IRQ $IRQ → CPU affinity mask: $ACTUAL (target: $MASK)"
else
    echo "WARNING: Could not find IRQ for $INTERFACE"
    echo "Available CAN-related interrupts:"
    grep -i "can\|flexcan\|mcan" /proc/interrupts 2>/dev/null || echo "  (none)"
fi
echo ""

# 6. CAN 错误统计
echo "=== CAN Error Counters ==="
if [ -f /proc/net/can/stats ]; then
    cat /proc/net/can/stats
else
    ip -statistics link show "$INTERFACE" | grep -A5 "can"
fi
echo ""

echo "=== Ready ==="
echo "Run: sudo ./new_TcpCan_server"
