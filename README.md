# 智能输液守护系统 / Smart Infusion Guardian System

**项目级别：** 2024 年合肥工业大学校级大学生创新创业训练计划项目，已完成验收  
**获奖：** 同年“挑战杯”团队省级一等奖  
**平台：** STM32F103C8T6 + ESP32 + 红外滴速传感器 + L298N + 蠕动泵/阀门 + OLED + Blinker 手机端

> 这份 README 面向第一次接触 STM32/ESP32 的同学，说明系统怎么工作、每个文件夹负责什么、代码从哪里开始看，以及复现时最容易踩的坑。  
> This README is a beginner-friendly guide to the system architecture, folder layout, code flow, and known issues.

---

## 1. 项目简介

智能输液守护系统是一个“输液滴速监测 + 自动调节 + 手机远程监控”的原型系统：

- **STM32F103C8T6** 负责实时任务：采集红外滴速传感器、计算每分钟滴数、控制蠕动泵和阀门、驱动 OLED、蜂鸣器提示。
- **ESP32** 负责联网任务：连接 Wi-Fi，运行 Blinker 手机端控件，提供 Web 配网页面，并通过串口与 STM32 交换数据。
- **手机端** 使用 Blinker App 查看当前滴速、设定目标滴速、开关输液、查看/解除报警。
- **执行机构** 使用 L298N 双路电机驱动模块控制蠕动泵和阀门。
- **电源/结构** 使用自定义 PCB V1.0 与 3D 打印/结构件，相关原理图和 Gerber 在 `docs/pcb`。

项目完成了校级大创验收，并获同年“挑战杯”团队省级一等奖。

## 2. 系统架构

```text
                 ┌──────────────────────────────┐
                 │        手机 Blinker App        │
                 │  开关 / 目标滴速 / 当前滴速     │
                 └───────────────┬──────────────┘
                                 │ Wi-Fi
                 ┌───────────────▼──────────────┐
                 │            ESP32              │
                 │  Wi-Fi + Blinker + Web 配网    │
                 │  UART2: GPIO16/17  115200     │
                 └───────────────┬──────────────┘
                                 │ 串口协议
                 ┌───────────────▼──────────────┐
                 │       STM32F103C8T6           │
                 │  滴速采集 / 泵阀控制 / OLED     │
                 │  USART2: PA2/PA3  115200      │
                 └───────┬───────────────┬──────┘
                         │               │
                ┌────────▼──────┐  ┌─────▼─────────┐
                │ 红外滴速传感器 │  │ L298N + 泵/阀  │
                │ EXTI 上升沿   │  │ 软件 PWM       │
                └───────────────┘  └───────────────┘
                         │
                ┌────────▼──────┐
                │ OLED / 蜂鸣器  │
                └───────────────┘
```

## 3. 仓库结构

```text
.
├─ firmware/
│  ├─ stm32/                         # STM32F103C8T6 Keil MDK 工程
│  │  ├─ User/main.c                 # 主逻辑：滴速、泵阀、串口命令、闭环
│  │  ├─ Hardware/
│  │  │  ├─ CountSensor.c/.h         # 红外滴速传感器 + TIM2 计时 + 滑动窗口
│  │  │  ├─ L298N.c/.h               # 双路电机/泵阀方向 + 软件 PWM
│  │  │  ├─ Serial.c/.h              # USART2 收发与 @ 协议
│  │  │  ├─ OLED.c/.h、OLED_Font.h   # 软件 I2C OLED
│  │  │  └─ Timer.c/.h               # TIM4 100 ms 心跳
│  │  ├─ System/Delay.c/.h           # 延时函数
│  │  ├─ Start/                      # 启动文件、CMSIS、STM32F10x 头文件
│  │  ├─ Library/                    # STM32F10x 标准外设库
│  │  ├─ Project.uvprojx             # Keil 工程文件（打开的入口）
│  │  └─ keilkill.bat                # Keil 临时文件清理脚本
│  └─ esp32/                         # ESP32 PlatformIO/Arduino 工程
│     ├─ src/main.cpp                # Wi-Fi、Blinker、Web 配网、串口桥接
│     ├─ platformio.ini              # 开发板、框架、依赖、上传串口
│     ├─ lib/Blinker-0.3.10230510/   # 本地 Blinker 库
│     ├─ include/、test/             # PlatformIO 预留目录
│     └─ .gitignore                  # PlatformIO 默认忽略规则
├─ docs/
│  ├─ references/                    # 输液传感器/监控系统/专利/物理原理 PDF
│  ├─ design/                        # 系统思维导图 .mind + 在线链接
│  └─ pcb/
│     └─ V1.0/                       # 原理图 PDF、Gerber ZIP、电路板参数
├─ README.md
├─ .gitignore
└─ .gitattributes
```

### 各目录负责对象

| 目录/文件 | 负责对象 | 说明 |
|---|---|---|
| `firmware/stm32/User/main.c` | STM32 主控 | 程序入口，初始化各模块，主循环读取滴速、执行泵阀控制、解析串口命令。 |
| `firmware/stm32/Hardware/CountSensor.c` | 滴速采集 | EXTI 上升沿触发，TIM2 记录两次滴落间隔，5 点滑动窗口求平均，换算每分钟滴数。 |
| `firmware/stm32/Hardware/L298N.c` | 泵/阀执行 | 两路方向 GPIO + TIM3 软件 PWM，控制蠕动泵和阀门。 |
| `firmware/stm32/Hardware/Serial.c` | STM32 串口 | USART2 中断接收，`@CMD\r\n` 协议，提供 `Serial_Printf`。 |
| `firmware/stm32/Hardware/OLED.c` | 本地显示 | PB8/PB9 软件 I2C，显示设备号、平均间隔、每分钟滴数。 |
| `firmware/stm32/Hardware/Timer.c` | 控制节拍 | TIM4 每 100 ms 触发一次，累计 10 次用于约 1 s 的闭环调节。 |
| `firmware/esp32/src/main.cpp` | ESP32 联网 | Wi-Fi 连接/AP 配网、Blinker 控件回调、串口收发、JSON 解析。 |
| `firmware/esp32/platformio.ini` | ESP32 构建 | `espressif32` + Arduino 框架，`ArduinoJson` 依赖，上传串口。 |
| `docs/references` | 调研资料 | 输液传感器、静脉输液监控、专利和物理原理。 |
| `docs/design` | 设计思路 | 思维导图 `.mind` 和在线链接：<https://mm.tt/app/map/3527921843?t=4ugoEBNRHv>。 |
| `docs/pcb/V1.0` | 硬件设计 | 原理图、Gerber、板子尺寸/结构参数。 |

## 4. 硬件与引脚

### 4.1 STM32F103C8T6

| 功能 | 引脚/资源 | 说明 |
|---|---|---|
| 滴速传感器 | PA11 / EXTI11 | 上升沿触发；代码中 `GPIO_EXTILineConfig` 使用 GPIOA。 |
| 蠕动泵 | PB15 / PB14 | L298N 第一路方向控制。 |
| 阀门 | PB13 / PB12 | L298N 第二路方向控制。 |
| 泵/阀 PWM | TIM3 中断 | 软件 PWM，`timvalue` 0–100 与 CCR 比较。 |
| 串口 | USART2：PA2 TX / PA3 RX | 115200，8N1，与 ESP32 交叉连接。 |
| OLED | PB8 SCL / PB9 SDA | 软件 I2C；PB6/PB7 为 OLED 电源控制。 |
| 蜂鸣器 | PB11 | 低电平触发（代码中先拉低再拉高）。 |
| LED | PB1 | 开漏输出，代码中用于状态指示。 |
| 心跳定时 | TIM4 | 100 ms 中断一次，`tim1_heartbeats` 累加。 |

> 注意：`main.c` 顶部还定义了 `TXD PA_9`、`RXD PA_10`，但 `Serial.c` 实际使用的是 **USART2 PA2/PA3**。接线时以 `Serial.c` 为准。

### 4.2 ESP32

| 功能 | 引脚/资源 |
|---|---|
| 与 STM32 串口 | UART2：GPIO17 TX / GPIO16 RX，115200 |
| Wi-Fi | 板载 |
| 手机端 | Blinker Wi-Fi 接入 |
| 配网 | 连接失败时开 AP：`ESP32_Config` / `12345678`，网页 `192.168.4.1` |

## 5. 工作逻辑

### 5.1 滴速检测

1. 红外对射/滴速传感器每滴一滴产生一个脉冲，接在 PA11。
2. `CountSensor_Init()` 配置 EXTI11 上升沿中断，并启动 TIM2（1 ms 计数）。
3. 每次中断：
   - 读取 TIM2 溢出计数作为两次滴落的时间间隔；
   - 小于 180 ms 的间隔视为抖动/干扰，丢弃；
   - 否则写入 5 点滑动窗口。
4. `CounterSensor_GetAvgValue()` 求窗口平均间隔。
5. `CountSensor_GetCountPerMinute()` 用 `60000 / 平均间隔(ms)` 换算每分钟滴数。
6. 主循环实时刷新 OLED 的 `Avg` 和 `Spd`。

> 目前代码在每次滴落时都会 `beep(100)`，是“滴落提示音”，不是“输液结束报警”。真正的“液体空了”检测在当前 STM32 代码里没有实现，ESP32 端虽然有 `"EMPTY"` 分支，但 STM32 不会发送这个字符串。

### 5.2 泵/阀控制

`L298N.c` 用两路 H 桥控制泵和阀门：

- `OFF`：两路方向都低，电机停止。
- `ZHENG`：IN1=1、IN2=0，正转。
- `FAN`：IN1=0、IN2=1，反转。

`pump_set(direct, speed)` 和 `valve_set(direct, speed)` 会同时更新方向和 PWM 占空比。`L298N_SetCompare()` 只保存 CCR，真正的 PWM 在 `TIM3_IRQHandler()` 里用 `timvalue` 与 CCR 比较产生。

> 重要：当前 `Tim3_Init()` 的 `PSC=7200-1`、`ARR=10000-1`，按 72 MHz 计算 TIM3 更新频率约为 **1 Hz**；`timvalue` 0–100 循环，软件 PWM 周期约为 **100 s**。这组参数对泵/阀来说非常慢，复现时建议重新核对 TIM3 的 PSC/ARR，或改用硬件 PWM 通道。

### 5.3 闭环调节

`User/main.c` 的 `infusion_loop()`：

1. 如果收到串口命令：
   - `@switch`：切换 `infusion_state`（0 停止 / 1 运行）。
   - `@set_speed`：提示输入 20–400 的目标滴速，解析后写入 `speed_required`。
2. 如果 `infusion_state == 0`：泵停止，阀门执行关闭动作。
3. 如果 `infusion_state == 1`：
   - 每 10 个 TIM4 心跳（约 1 s）比较一次 `speed_required` 与 `count_per_min`；
   - 实际滴速低于目标：`pump_speed += 2`，上限 100；
   - 实际滴速高于目标：`pump_speed -= 2`，下限 20；
   - 调用 `pump_set()` 更新泵速。
4. 每 1 s 通过串口打印一次 `Count / Pump / Valve` 调试信息。

这是一个简单的 **bang-bang/比例式** 调节，不是严格 PID。它的优点是简单、直观；缺点是目标附近可能来回抖动，且泵/阀的机械死区、软管弹性、液位高度都会影响实际流量。

### 5.4 STM32 ↔ ESP32 串口协议

STM32 端使用 `@` 开头、`\r\n` 结尾的文本协议。`isStrEqual()` 从字符串第 1 个字符开始比较，所以发送时必须带 `@`：

| 方向 | 数据 | 作用 |
|---|---|---|
| ESP32 → STM32 | `@ESP\r\n` | ESP32 请求握手（STM32 当前代码未回复 `STM`）。 |
| ESP32 → STM32 | `@switch\r\n` | 切换输液开/关。 |
| STM32 → ESP32 | `State:0` / `State:1` | 返回当前开关状态（文本，不是 JSON）。 |
| ESP32 → STM32 | `@set_speed\r\n` | 请求设置目标滴速。 |
| STM32 → ESP32 | `Please input the speed(20 - 400):` | 提示输入目标值。 |
| ESP32 → STM32 | `@<数字>\r\n` | 目标滴速，STM32 限制 20–400。 |
| STM32 → ESP32 | `The speed is adjusted successfully:<n>` | 设置成功。 |
| ESP32 → STM32 | `@acquire\r\n` | ESP32 请求数据（STM32 当前代码未实现）。 |
| STM32 → ESP32 | `Count:<n>, Pump:<n>, Valve:<n>` | 当前调试信息（文本，不是 JSON）。 |

> **当前仓库代码存在协议不一致：** ESP32 端按 JSON 设计（`state`、`speed`、`requiredspeed`），会发送 `@acquire` 并等待 JSON；STM32 端当前只实现了 `@switch`、`@set_speed` 和文本回传，没有 `@acquire`/JSON/`EMPTY` 处理。复现时请先统一协议，否则 ESP32 会卡在 `STM_Init()` 或 `STM_ReadData()` 的等待循环里。这是这份代码最需要注意的地方。

### 5.5 ESP32：Wi-Fi、Blinker 与 Web 配网

`firmware/esp32/src/main.cpp` 的上电流程：

1. `Serial.begin(115200)` 打开调试串口。
2. `STM_Serial.begin(115200, SERIAL_8N1, 16, 17)` 打开与 STM32 的 UART2。
3. `STM_Init()` 循环发送 `@ESP`，等待 STM32 回复 `STM`（当前 STM32 代码未实现，见上面的协议问题）。
4. `Wifi_init_process()`：
   - 从 EEPROM 地址 2500/2532 读取 SSID/密码；
   - 连接 Wi-Fi，最多尝试 10 次；
   - 失败则进入 AP 模式，SSID `ESP32_Config`、密码 `12345678`，浏览器访问 `192.168.4.1` 打开配网页面；
   - 保存后 `ESP.restart()` 重启。
5. `Blinker.begin(key, ssid, pswd)` 启动 Blinker。
6. 注册控件回调：
   - `btn_switch`：开关输液；
   - `sp_adj`：滑块设置目标滴速；
   - `btn_alarm`：解除报警；
   - `btn_ref`：手动刷新；
   - `num_spd` / `num_req`：显示当前/目标滴速。
7. `loop()` 中 `Blinker.run()`，并读取 STM32 串口；如果收到 `"EMPTY"` 且未报警，则 `Blinker.push("输液已结束")`。

> 安全提示：原 `main.cpp` 中的 Blinker 设备密钥已替换为 `YOUR_BLINKER_KEY`，避免把私人密钥上传到公开仓库。运行前请填入你自己的 Blinker 密钥。原文件备份在仓库外的 `大创项目整理_excluded/original/main.cpp.with_blinker_key`。

## 6. 构建与运行

### 6.1 STM32（Keil MDK）

1. 安装 Keil MDK5、STM32F1xx Device Family Pack、ST-Link 驱动。
2. 打开 `firmware/stm32/Project.uvprojx`。
3. 编译（Build），用 ST-Link 下载。
4. 串口 USART2（PA2/PA3）接 USB-TTL 或 ESP32，115200 8N1。
5. 上电后 OLED 显示设备号、平均间隔、每分钟滴数；蜂鸣器在每次滴落时短鸣。

> `Objects/`、`Listings/`、`DebugConfig/`、`Project.uvoptx` 等构建/调试产物已排除，Keil 会在本地重新生成。

### 6.2 ESP32（PlatformIO）

1. 安装 VS Code + PlatformIO IDE。
2. 打开 `firmware/esp32`（包含 `platformio.ini`）。
3. 修改 `platformio.ini`：
   - `upload_port = COM22` 改成你自己的串口，或删除这一行让 PlatformIO 自动检测；
   - `board = upesy_wroom` 是当前配置；如果实际使用 ESP32-C3，请改成对应 board。
4. 在 `src/main.cpp` 填入自己的 Blinker key。
5. 编译/上传：PlatformIO 的 Build / Upload，或命令行 `pio run -t upload`。
6. 首次配网：连不上 Wi-Fi 时，手机连接 `ESP32_Config`（密码 `12345678`），浏览器打开 `192.168.4.1` 填 SSID/密码。

## 7. 复现注意事项与已知问题

1. **串口协议不一致（最重要）**：ESP32 端期待 JSON 和 `@acquire`，STM32 端只实现了文本命令。复现前先统一协议，建议在 STM32 增加 `@acquire` 分支，按 `{"state":0,"speed":80,"requiredspeed":80}` 格式回传。
2. **握手不一致**：ESP32 `STM_Init()` 等待 `STM`，STM32 上电只打印 `Start!`。可在 STM32 的 `Serial_Init()` 后发送 `STM\r\n`，或修改 ESP32 握手逻辑。
3. **滴速传感器引脚冲突**：`CountSensor_Init()` 把 GPIOB Pin 11 配成上拉输入，但 `GPIO_EXTILineConfig` 用的是 GPIOA Pin 11，ISR 读的也是 PA11；而 PB11 同时被 `buzzer_init()` 配成蜂鸣器输出。实际硬件应确认传感器接 PA11 还是 PB11，并把 GPIO 初始化、EXTI 映射、ISR 读取统一到同一端口。
4. **软件 PWM 周期偏大**：TIM3 的 PSC/ARR 计算约为 1 Hz 更新、100 级 PWM 周期约 100 s，泵/阀响应会很慢。建议核对定时器参数或改用硬件 PWM。
5. **除零风险**：`CountSensor_GetCountPerMinute()` 直接 `60000 / avg`，如果还没有滴落、`avg == 0` 会除零。建议加 `if (avg == 0) return 0;`。
6. **阻塞等待**：ESP32 的 `STM_Init()`、`STM_ReadData()`、`STM_Setspeed()`、`STM_Switch()` 都是无限 `while(1)` 等待串口，没有超时；STM32 不应答时会卡死。建议加超时和重试。
7. **滴速范围不一致**：ESP32 `STM_Setspeed()` 限制 10–120，STM32 `set_speed` 限制 20–400。请统一范围。
8. **无液位检测**：当前 STM32 没有“空液/无滴落”报警逻辑，只有每次滴落的蜂鸣提示；ESP32 的 `"EMPTY"` 分支需要 STM32 侧补充发送。
9. **源码编码混用**：STM32 用户代码多为 UTF-8，部分标准库文件为 GBK；若中文注释乱码，请在编辑器中切换 GBK/GB18030 或 UTF-8。
10. **机械/流体标定**：滴速与泵 PWM、软管直径、液位高度强相关，换泵/换管后必须重新标定 `pump_speed` 与 `count_per_min` 的关系。

## 8. 资料与许可

- `docs/references` 中的论文/专利仅用于学习调研，版权归原作者/权利人所有。
- `firmware/esp32/lib/Blinker-0.3.10230510` 为 Blinker 官方库，遵循其原始许可证。
- STM32 标准外设库版权归 STMicroelectronics。
- 本项目的应用层代码用于学习、交流和后续大创/竞赛参考，请保留原作者与资料出处。

---

## English Summary

This repository contains the **Smart Infusion Guardian System**, a 2024 Hefei University of Technology university-level innovation project that passed acceptance and won the **Provincial First Prize** in the “Challenge Cup” team track.

The system uses an **STM32F103C8T6** as the real-time controller and an **ESP32** as the Wi-Fi/Blinker gateway:

- The STM32 reads an infrared drip sensor on PA11 (EXTI rising edge), measures the interval between drops with TIM2, applies a 5-sample sliding window, and computes drops per minute.
- It drives a peristaltic pump and valve through an L298N dual H-bridge with software PWM, and runs a simple 1-second bang-bang/proportional speed update.
- It displays data on an OLED (PB8/PB9 software I2C) and beeps on each detected drop.
- The ESP32 connects to Wi-Fi, runs the Blinker app widgets, provides a Web AP configuration page (`ESP32_Config` / `12345678` / `192.168.4.1`), and bridges UART2 (GPIO16/17, 115200) to the STM32.
- The PCB, schematic, Gerber, design mind map, and reference papers are under `docs/`.

Key folders:

- `firmware/stm32` — Keil MDK project: `User/main.c`, `Hardware/` (drip sensor, L298N, OLED, UART, timer), `System/`, `Start/`, `Library/`.
- `firmware/esp32` — PlatformIO project: `src/main.cpp`, `platformio.ini`, local Blinker library.
- `docs/references` — infusion sensor/monitoring papers and patents.
- `docs/design` — system mind map.
- `docs/pcb/V1.0` — schematic, Gerber, board parameters.

Important notes:

- The Blinker device key in the original source has been replaced with `YOUR_BLINKER_KEY`; fill in your own key before running.
- The current ESP32 and STM32 code have **protocol mismatches**: the ESP32 expects JSON and `@acquire`, while the STM32 only implements text commands (`@switch`, `@set_speed`) and text responses. Align the protocol before reproduction.
- The drip sensor GPIO initialization, EXTI mapping, and ISR port are inconsistent (GPIOB PB11 init vs GPIOA PA11 EXTI/ISR); verify the actual hardware wiring.
- The TIM3 software PWM timing is very slow (about 1 Hz update, ~100 s for 100 steps); review PSC/ARR or use hardware PWM.
- `count_per_min` divides by the average interval without a zero check; add a guard.
- ESP32 serial waits are blocking without timeouts; add timeouts/retries.
- ESP32 speed range (10–120) and STM32 speed range (20–400) differ; unify them.
- There is no real “empty bag” detection in the STM32 code; the ESP32 `"EMPTY"` branch needs STM32 support.
- Source encodings are mixed (UTF-8 user code, some GBK standard-library files); switch encoding if Chinese comments are garbled.

The application code is shared for learning and future competition reference. Please keep the original author and data-source credits.
