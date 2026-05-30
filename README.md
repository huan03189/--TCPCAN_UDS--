# 面向飞腾平台的 TCP/CAN 双模 UDS 诊断系统

第二十一届中国研究生电子设计竞赛作品

## 目录结构

```
├── src/                          # 完整源代码
│   ├── new_TcpCan_client/        # Qt 客户端（Windows）
│   └── new_TcpCan_server/        # 服务端（ARM Linux）
├── bin/
│   ├── windows/                  # 客户端可执行 + 依赖 DLL
│   │   └── new_TcpCan_client.exe
│   └── arm-linux/                # 飞腾派可执行
│       ├── tcpcan-server
│       ├── setup_can_irq.sh      # CAN 初始化 + IRQ 绑定脚本
│       └── tcpcan-server.service # systemd 服务文件
├── 技术论文.docx
└── README.md
```

## 快速部署

### 客户端（Windows）

**直接运行：**

`bin\windows\new_TcpCan_client.exe`

**从源码编译：**

1. 安装 Qt 6.8（MSVC 2022 64-bit）
2. 打开 `src\new_TcpCan_client\new_TcpCan_client.pro`
3. 构建 → Release 模式

### 服务端（飞腾派 ARM Linux）

**直接部署二进制：**

```bash
# 拷贝到飞腾派
scp bin/arm-linux/tcpcan-server user@phytiumpi:~/
scp bin/arm-linux/setup_can_irq.sh user@phytiumpi:~/
scp bin/arm-linux/tcpcan-server.service user@phytiumpi:~/

# 在飞腾派上
sudo cp tcpcan-server /usr/local/bin/
sudo cp setup_can_irq.sh /usr/local/bin/
sudo cp tcpcan-server.service /etc/systemd/system/

# 修改 CAN 接口（如使用 can2）
sudo sed -i 's/CAN_IFACE=can0/CAN_IFACE=can2/' /etc/systemd/system/tcpcan-server.service

# 启用开机自启
sudo systemctl daemon-reload
sudo systemctl enable tcpcan-server
sudo systemctl start tcpcan-server
```

**从源码编译：**

```bash
cd src/new_TcpCan_server
qmake && make -j$(nproc)
sudo cp new_TcpCan_server /usr/local/bin/tcpcan-server
```

## 运行

```bash
# 服务端（飞腾派）
./tcpcan-server -c can0 -p 8888

# 客户端（Windows）
new_TcpCan_client.exe
```

客户端启动后：
1. **TCP 面板** → 输入飞腾派 IP + 端口 → 连接
2. **CAN 面板** → 驱动选 `peakcan`，接口选 `usb0` → 初始化
3. **UDS 面板** → 选择通道 → 进行安全访问 → 开始诊断
