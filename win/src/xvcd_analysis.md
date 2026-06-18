# xvcd (Xilinx Virtual Cable Daemon) — Windows 版源码分析

## 1. 项目概述

xvcd 是一个运行在 **Windows 平台上的 XVC (Xilinx Virtual Cable) 服务器**。
它通过 TCP 网络接收 Xilinx Vivado / impact 等工具的 JTAG 命令，然后通过 **FTDI FT2232 芯片**的同步位 bang 模式驱动 JTAG 信号，实现对目标 FPGA/芯片的 JTAG 调试与编程。

---

## 2. 项目文件结构

```
win/src/
├── xvcd.c              # 主程序入口，TCP 服务器 + XVC 协议处理
├── io_ftdi.h            # IO 层接口声明 (3个函数)
├── io_ftdi.c            # FTDI 芯片驱动层实现
├── stdafx.cpp           # VC++ 预编译头
├── stdafx.h             # VC++ 预编译头
├── targetver.h          # Windows SDK 版本
├── xvcd2.sln            # Visual Studio 解决方案文件
├── xvcd2.vcxproj        # Visual Studio 工程文件
└── xvcd2.vcxproj.user   # Visual Studio 用户配置
```

---

## 3. 模块分析

### 3.1 主模块 — `xvcd.c` (401行)

#### `main()` — 程序入口

1. 调用 `QueryPerformanceFrequency()` 初始化性能计数器
2. 调用 `io_init()` 初始化 FTDI 设备
3. 初始化 Winsock，创建 TCP socket，绑定端口 **2542**
4. 进入 `select()` 事件循环：
   - 监听新连接（listen socket）
   - 接收新客户端，存入 fd_set
   - 对每个已连接客户端调用 `handle_data()` 处理
5. 退出时调用 `io_close()` 关闭 FTDI

#### `handle_data(fd)` — XVC 协议处理核心

循环接收客户端命令，直到满足条件（`seen_tlr && jtag_state == run_test_idle`）才退出，这样设计是为了在多个客户端之间安全切换。

支持 3 种命令：

| 命令 | 协议匹配 | 功能 |
|------|----------|------|
| `getinfo` | 接收 `"ge"` + 6 字节 | 返回版本标识 `"xvcServer_v1.0:2048\n"`（2048 表示最大 shift bits） |
| `settck` | 接收 `"se"` + 9 字节 | 设置 TCK 频率（占位实现，仅回复 4 字节，未改变硬件频率） |
| `shift` | 接收 `"sh"` + 4 字节 | **核心命令**：读取 bit 长度和数据，调用 `io_scan()` 执行 JTAG 移位，返回 TDO 结果 |

`shift` 命令处理流程：

1. 读取 4 字节 `len`（bit 数）
2. 计算 `nr_bytes = (len + 7) / 8`，读取 `nr_bytes * 2` 字节的 TMS/TDI 数据
3. 逐 bit 驱动 JTAG 状态机前进（`jtag_step()`）
4. 调用 `io_scan()` 通过 FTDI 执行实际 JTAG 操作
5. 将 `nr_bytes` 字节的 TDO 结果发回客户端

#### `jtag_step(state, tms)` — JTAG 状态机

实现标准的 JTAG TAP **16 状态**（实际使用 16 个，枚举定义 `num_states`）状态转换表：

```
test_logic_reset ↔ run_test_idle
     ↓                    ↓
select_dr_scan     select_ir_scan
     ↓                    ↓
capture_dr          capture_ir
     ↓                    ↓
shift_dr            shift_ir
     ↓                    ↓
exit1_dr            exit1_ir
     ↓                    ↓
pause_dr            pause_ir
     ↓                    ↓
exit2_dr            exit2_ir
     ↓                    ↓
update_dr           update_ir
     ↓                    ↓
   run_test_idle（回到起点）
```

每个状态根据 TMS 高低选择两条路径之一。

#### `sread(fd, target, len)` — TCP 读取封装

确保从 socket 读取指定长度的字节，处理粘包问题。

---

### 3.2 IO 接口 — `io_ftdi.h` (4行)

声明三个函数接口，将上层逻辑与底层硬件实现解耦：

```c
int io_init(int vendor, int product);   // 初始化 FTDI 设备
int io_scan(const unsigned char *TMS, const unsigned char *TDI,
            unsigned char *TDO, int bits);  // 执行一次 JTAG 扫描
void io_close(void);                         // 关闭 FTDI 设备
```

---

### 3.3 FTDI 驱动层 — `io_ftdi.c` (146行)

#### `io_init()`

1. 打开第一个 FTDI 设备（`FT_Open(0, ...)`，固定索引 0，多设备需修改）
2. 配置为**同步位 bang 模式**（`FT_BitMode(..., FT_BITMODE_SYNC_BITBANG)`）
3. 清空收发缓冲区（`FT_Purge`）
4. 设置波特率 `3000000 / 16 = 187500`（同步位 bang 下，时钟频率 = 波特率）
5. 设置延迟定时器为 2ms（`FT_SetLatencyTimer`）
6. 设置 USB 收发缓冲区为 16KB（`FT_SetUSBParameters`）

#### `io_scan()`

将 TMS/TDI bit 流转换为 FTDI 位 bang 数据格式，通过 `FT_Write` 发送，再 `FT_Read` 读回 TDO 结果。

1. 构建输出 buffer：每个 JTAG bit 生成 2 字节（TCK 低 + TCK 高）
2. `FT_Write` 发送全部数据
3. 循环 `FT_Read` 读回等量数据
4. 从 buffer 的 TCK 高电平字节中提取 TDO 值

#### `io_close()`

调用 `FT_Close()` 关闭设备。

---

## 4. 数据流

```
Vivado / impact (TCP client)
      │
      │ TCP :2542 (XVC 协议)
      ▼
xvcd.c — handle_data()   ← 解析命令：getinfo / settck / shift
      │
      │ io_scan(TMS, TDI, TDO, bits)
      ▼
io_ftdi.c — FT_Write / FT_Read
      │
      │ USB
      ▼
FTDI FT2232 (同步位 bang 模式)
      │
      │ TCK, TMS, TDI, TDO
      ▼
目标 FPGA / 芯片 JTAG 链
```

---

## 5. 关键设计点

### 5.1 多客户端安全

XVC 协议允许远程调试，可能存在多个调试器同时连接。代码通过 `seen_tlr` 标志实现安全切换：

- 只有经过 `test_logic_reset` 状态后回到 `run_test_idle` 才允许断开当前连接
- 一旦进入 `capture_dr` 或 `capture_ir` 后，不允许退出（防止 IR/DR 被破坏）
- 这样确保一个客户端不会打断另一个客户端的 JTAG 操作

```c
seen_tlr = (seen_tlr || jtag_state == test_logic_reset)
        && (jtag_state != capture_dr)
        && (jtag_state != capture_ir);
```

### 5.2 Nagle 算法禁用

```c
setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, ...);
```

在服务端 socket 和客户端 socket 上都设置了 `TCP_NODELAY`，禁用 Nagle 算法以减少延迟，这对 JTAG 交互式操作非常关键。

### 5.3 虚假状态过滤

处理了 Xilinx impact 工具的一个怪异行为：在读取 IR/DR 后，impact 会额外插入一个 capture_ir/capture_dr 周期，错误地将 IR 设置为刚读出的值。代码通过特定模式检测并忽略这些事务：

```c
if ((jtag_state == exit1_ir && len == 5 && buffer[0] == 0x17)
 || (jtag_state == exit1_dr && len == 4 && buffer[0] == 0x0b))
{
    // 忽略
}
```

### 5.4 Buffer 限制

`xvcInfo` 中的 `:2048` 表示最大支持 2048 bits 的 JTAG 移位操作（即 256 字节），超过此长度客户端应分多次操作。

---

## 6. FTDI 位 bang 数据格式详解

### 6.1 端口映射

```c
#define PORT_TCK    0x01  // bit 0 — 时钟
#define PORT_TDI    0x02  // bit 1 — 数据输入
#define PORT_TDO    0x04  // bit 2 — 数据输出 (只读)
#define PORT_TMS    0x08  // bit 3 — 模式选择
#define IO_OUTPUT (PORT_TCK|PORT_TDI|PORT_TMS)  // 0x0B
```

### 6.2 每个 JTAG bit 对应 2 个 FTDI 字节

JTAG 协议在 TCK 上升沿采样数据。同步位 bang 模式下，FTDI 芯片以固定速率输出 GPIO 值，每个字节对应一次 8-bit 并行输出。

```c
for (int i = 0; i < bits; ++i)
{
    unsigned char v = 0;
    if (TMS[i/8] & (1<<(i&7))) v |= PORT_TMS;
    if (TDI[i/8] & (1<<(i&7))) v |= PORT_TDI;
    buffer[i * 2 + 0] = v;            // TCK=0: 设置 TMS/TDI
    buffer[i * 2 + 1] = v | PORT_TCK; // TCK=1: 拉高 TCK → 上升沿采样
}
```

### 6.3 时序示例

发送 **TMS=1, TDI=0** 的一个 JTAG bit：

| 字节偏移 | 值 (二进制) | 引脚状态 |
|----------|------------|----------|
| `buffer[0]` | `0000 1000` (0x08) | TCK=0, TMS=1, TDI=0 |
| `buffer[1]` | `0000 1001` (0x09) | TCK=1, TMS=1, TDI=0 |

波形示意：
```
TCK:  __/‾‾\__/‾‾\
TMS:  ‾‾‾‾‾‾‾‾‾‾‾‾  (保持高)
TDI:  ____________  (保持低)
      ↑       ↑
    第1字节  第2字节
    TCK=0   TCK=1 (上升沿 → 目标芯片采样 TMS/TDI)
```

### 6.4 TDO 读取

FPGA 在 TCK 上升沿同时输出 TDO，因此读取时取每个 bit 的第 2 个字节（TCK=1 时刻）：

```c
for (int i = 0; i < bits; ++i)
{
    if (buffer[i * 2 + 1] & PORT_TDO)  // TCK=1 时的 TDO
    {
        TDO[i/8] |= 1 << (i&7);
    }
}
```

---

## 7. 关于 "Bitbang" 名称的由来

**Bitbang** 是 **bit**（位）+ **bang**（拟声词，"砰"）的合成词。

### 字面含义

"Bang" 模拟的是**用锤子一下一下敲打**的声音，形象地描述了**一个 bit 一个 bit 地"砸"出去**的操作方式。

### 技术含义

Bitbang 模式指**直接用软件逐位控制 GPIO 引脚电平**，而不是通过硬件专用外设（如 SPI、JTAG、I2C 控制器）自动生成时序。

| 方式 | 比喻 | 特点 |
|------|------|------|
| **硬件控制器**（如硬件 SPI/JTAG） | 自动流水线 | 写入一个字节，硬件自动产生 8 个时钟脉冲 |
| **Bitbang 模式** | 手动一下一下敲 | 每条指令控制一个引脚的一次电平变化 |

### FTDI 的同步位 bang 模式

FTDI 芯片支持两种位 bang 模式：

1. **异步位 bang** — 纯软件控制，每个 bit 之间可由软件插入延迟
2. **同步位 bang** — FTDI 芯片内部用固定的波特率时钟自动输出 GPIO 值，每写一个字节，芯片就自动在时钟上升沿输出到引脚

虽然叫 "bang"，但实际上同步位 bang 是**硬件定时器驱动的自动 bang**，比纯软件 bitbang 快得多。

`FT_BITMODE_SYNC_BITBANG` = **"用硬件时钟同步、逐位敲打 GPIO 的模式"**

---

## 8. 编译依赖

- **FTDI D2XX 驱动**：`libFTDI/ftd2xx.h` 和 `ftd2xx.lib`（静态链接，通过 `#define FTD2XX_STATIC` 指定）
- **Winsock2**：`Ws2_32.lib`（通过 `#pragma comment(lib, "Ws2_32.lib")` 链接）
- **Visual Studio**：解决方案文件 `xvcd2.sln` 对应 Visual Studio 工程