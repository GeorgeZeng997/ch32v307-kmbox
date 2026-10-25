# CH32V307VCT6 KMBox Net 固件 (`ch32v307-kmbox`)

基于沁恒 **CH32V307VCT6**（RISC-V 青稞 V4F + 双路硬件 USB + 内置 10M 以太网 PHY）实现的零串口特征硬件键鼠合成器固件。

---

## 1. 硬件接线说明

| 开发板接口 | 内部外设与引脚 | 连接对象 | 作用 |
| :--- | :--- | :--- | :--- |
| **Type-C 口 1 (`USBFS`)** | `PA11 / PA12` (USB OTG FS) | **Type-C 公转 USB 母 OTG 转接头** $\rightarrow$ **物理有线/2.4G 无线鼠标** | **USB Host 主机**：自动解析 8/12/16-bit 鼠标报告、5 键与滚轮，支持动态克隆物理鼠标 `VID:PID` |
| **RJ45 网口 (`ETH`)** | 内置 10M PHY | **六类网线** $\rightarrow$ **USB 百兆网卡** $\rightarrow$ **电脑 USB 口** | **UDP Server (`192.168.2.188:8888`)**：接收电脑端 UDP 键鼠指令，电脑无任何 `COM` 串口产生 |
| **Type-C 口 2 (`USBHS`)** | `PB6 / PB7` (USBHS) | **Type-C 数据线** $\rightarrow$ **电脑 USB 口** | **1000Hz 单接口 USB HID 鼠标**：硬件级矢量叠加（物理鼠标 + UDP 网络位移 + 余数顺延）与按键按位或合成 |

---

## 2. 电脑端 USB 网卡 IP 设置

将 USB 网卡插在电脑上并用网线连接开发板 RJ45 网口后，将电脑上该 **USB 网卡（以太网适配器）的 IPv4 地址** 设为静态 IP：
- **IP 地址**：`192.168.2.100`（`192.168.2.x` 网段内除 `.188` 外的任意地址均可）
- **子网掩码**：`255.255.255.0`
- **默认网关**：留空即可（切勿填写网关，以免影响电脑正常上网）

设置完成后可在终端执行 `ping 192.168.2.188` 验证连通性。

---

## 3. UDP 通信协议 (`192.168.2.188:8888`)

### 3.1 二进制高速协议（推荐，固定 9 字节，小端序）

```c
typedef struct __attribute__((packed)) {
    uint8_t  head1;     // 0x55
    uint8_t  head2;     // 0xAA
    uint8_t  buttons;   // Bit0:左键, Bit1:右键, Bit2:中键, Bit3:侧键后退, Bit4:侧键前进
    int16_t  dx;        // X 轴有符号 16 位相对位移 (Little-Endian)
    int16_t  dy;        // Y 轴有符号 16 位相对位移 (Little-Endian)
    int8_t   wheel;     // 滚轮有符号 8 位位移
    uint8_t  checksum;  // (buttons + dx_lo + dx_hi + dy_lo + dy_hi + wheel) & 0xFF
} KmboxUdpPacket;
```

### 3.2 兼容文本协议

同时向下兼容 `pico-mouse` 的明文格式：
```text
55 <btn> <dx> <dy> <wheel> <pan> <checksum>\n
```

---

## 4. 一键刷机步骤 (`WCHISPTool`)

1. 下载并打开沁恒官方刷机工具 **WCHISPTool**，切换到 **「CH32V 系列 (RISC-V)」** 标签页，芯片型号选择 **`CH32V307`**，下载方式选择 **USB**。
2. **首次烧录注意**：在「目标芯片配置」中，确保 **FLASH / RAM 配比** 选择为 **`256KB FLASH + 64KB RAM`**。
3. 按住开发板上的 **`BOOT0`** 按键不放，将开发板任意 Type-C 口插入电脑（或按一下 `RST` 复位键），松开 `BOOT0`，工具会自动识别出 USB 设备。
4. 在「用户程序文件」中选择编译好的 `ch32v307_kmbox.hex`（或 `ch32v307_kmbox.bin`），点击 **下载** 即可完成刷机。
