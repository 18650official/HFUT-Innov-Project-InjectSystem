# 智能输液守护系统 / Smart Infusion Guardian System

**项目级别：** 2024 年合肥工业大学校级大学生创新创业训练计划项目，已完成验收  
**获奖：** 同年“挑战杯”团队省级一等奖  
**平台：** STM32F103C8T6 + ESP32 + 红外滴速传感器 + L298N + 蠕动泵/阀门 + OLED + 旋转编码器 + Blinker 手机端

> 这份 README 面向第一次接触 STM32/ESP32 的同学，说明系统怎么工作、每个文件夹负责什么、代码从哪里开始看，以及复现时最容易踩的坑。  
> This README is a beginner-friendly guide to the system architecture, folder layout, code flow, and known issues.

---

## 1. 项目简介

智能输液守护系统是一个“输液滴速监测 + 自动调节 + 空液报警 + 手机远程监控”的原型系统：

- **STM32F103C8T6** 负责实时任务：采集红外滴速传感器、计算每分钟滴数、闭环调节阀门、驱动 OLED、蜂鸣器提示、读取旋转编码器，并用 **cJSON** 打包 JSON 数据。
- **ESP32** 负责联网任务：连接 Wi-Fi，运行 Blinker 手机端控件，提供 Web 配网页面；通过串口向 STM32 发送文本命令，并用 **ArduinoJson** 解析 STM32 返回的 JSON。
- **手机端** 使用 Blinker App 查看当前滴速、设定目标滴速、开关输液、查看/解除报警。
- **执行机构** 使用 L298N 双路电机驱动模块控制蠕动泵和阀门；当前闭环实际驱动的是**阀门**，泵通道保留备用。
- **本地交互** 旋转编码器可现场调节目标滴速、开关输液，并在空液报警时按键复位。
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
                    │  ArduinoJson 解析 STM32 JSON   │
                    └───────────────┬──────────────┘
                                    │ @命令 / JSON
                    ┌───────────────▼──────────────┐
                    │       STM32F103C8T6           │
                    │  USART3 PB10/PB11 ↔ ESP32     │
                    │  USART2 PA2/PA3   ↔ PC 调试    │
                    │  滴速采集 / 阀门控制 / OLED     │
                    │  cJSON / 编码器 / 空液报警      │
                    └───┬─────────┬──────────┬──────┘
                        │         │          │
              ┌─────────▼──┐ ┌────▼──────┐ ┌─▼────────────┐
              │ 红外滴速传感器│ │L298N 阀门  │ │OLED / 蜂鸣器  │
              │ PA11 EXTI  │ │泵(备用)    │ │编码器 PA5/6/7 │
              └────────────┘ └───────────┘ └──────────────┘
```

> 注意：STM32 有两路串口。**USART2（PA2/PA3）** 用于电脑调试/文本命令；**USART3（PB10/PB11）** 用于和 ESP32 通信。二者使用各自独立的接收状态机与缓冲区。

## 3. 仓库结构

```text
.
├─ firmware/
│  ├─ stm32/                         # STM32F103C8T6 Keil MDK 工程
│  │  ├─ User/main.c                 # 主逻辑：滴速、阀门闭环、串口命令、JSON、报警
│  │  ├─ Hardware/
│  │  │  ├─ CountSensor.c/.h         # 红外滴速传感器 + TIM2 计时 + 滑动窗口
│  │  │  ├─ L298N.c/.h               # 双路电机/泵阀方向 + TIM3 软件 PWM + valve_set/pump_set
│  │  │  ├─ Serial.c/.h              # USART2 调试口与 USART3(ESP) 收发，@ 协议
│  │  │  ├─ OLED.c/.h、OLED_Font.h   # 软件 I2C OLED
│  │  │  ├─ Buzzer.c/.h              # 蜂鸣器(PA1) / LED(PB1)
│  │  │  ├─ Encoder.c/.h             # 旋转编码器增量 + 按键
│  │  │  └─ Timer.c/.h               # TIM4 100 ms 心跳
│  │  ├─ System/
│  │  │  ├─ Delay.c/.h               # 延时函数
│  │  │  └─ cJSON.c/.h               # 轻量级 JSON 库（STM32 端打包 JSON）
│  │  ├─ Start/                      # 启动文件、CMSIS、STM32F10x 头文件
│  │  ├─ Library/                    # STM32F10x 标准外设库
│  │  ├─ Project.uvprojx             # Keil 工程文件（打开的入口）
│  │  └─ keilkill.bat                # Keil 临时文件清理脚本
│  └─ esp32/                         # ESP32 PlatformIO/Arduino 工程
│     ├─ src/main.cpp                # Wi-Fi、Blinker、Web 配网、串口桥接、JSON 解析
│     ├─ platformio.ini              # 开发板、框架、依赖、上传串口
│     ├─ lib/Blinker-0.3.10230510/   # 本地 Blinker 库（内含 ArduinoJson）
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
| `firmware/stm32/User/main.c` | STM32 主控 | 程序入口，初始化各模块；主循环刷新滴速、处理两路串口命令、编码器输入、阀门闭环与空液报警；用 cJSON 打包数据。 |
| `firmware/stm32/Hardware/CountSensor.c` | 滴速采集 | EXTI 上升沿触发，TIM2 记录两次滴落间隔，5 点滑动窗口求平均，换算每分钟滴数。 |
| `firmware/stm32/Hardware/L298N.c` | 泵/阀执行 | 两路方向 GPIO + TIM3 软件 PWM；`valve_set()` 控制阀门，`pump_set()` 控制泵（当前未参与闭环）。 |
| `firmware/stm32/Hardware/Serial.c` | 串口通信 | USART2 调试口与 USART3(ESP) 各自独立中断收包，`@CMD\r\n` 协议，提供 `Serial_Printf` / `ESP_Serial_Printf`。 |
| `firmware/stm32/Hardware/OLED.c` | 本地显示 | PB8/PB9 软件 I2C；显示平均间隔、滴速、开关状态与目标滴速；报警时显示 `Empty!`。 |
| `firmware/stm32/Hardware/Buzzer.c` | 声音/指示灯 | PA1 蜂鸣器（低电平触发）、PB1 开漏 LED。 |
| `firmware/stm32/Hardware/Encoder.c` | 本地输入 | PA5/PA6 正交编码器计数，PA7 按键；用于现场调速与开关/复位。 |
| `firmware/stm32/Hardware/Timer.c` | 控制节拍 | TIM4 每 100 ms 触发一次，累计 10 次用于约 1 s 的闭环调节。 |
| `firmware/stm32/System/cJSON.c` | JSON 打包 | STM32 端把 `state` / `speed` / `requiredspeed` 序列化为 JSON。 |
| `firmware/esp32/src/main.cpp` | ESP32 联网 | Wi-Fi 连接/AP 配网、Blinker 控件回调、串口收发、ArduinoJson 解析。 |
| `firmware/esp32/platformio.ini` | ESP32 构建 | `espressif32` + Arduino 框架，`ArduinoJson` 依赖，上传串口。 |
| `docs/references` | 调研资料 | 输液传感器、静脉输液监控、专利和物理原理。 |
| `docs/design` | 设计思路 | 思维导图 `.mind` 和在线链接：<https://mm.tt/app/map/3527921843?t=4ugoEBNRHv>。 |
| `docs/pcb/V1.0` | 硬件设计 | 原理图、Gerber、板子尺寸/结构参数。 |

## 4. 硬件与引脚

### 4.1 STM32F103C8T6

| 功能 | 引脚/资源 | 说明 |
|---|---|---|
| 滴速传感器 | PA11 / EXTI11 | 上升沿触发；`GPIO_EXTILineConfig` 与 ISR 均使用 GPIOA Pin 11。 |
| 阀门（L298N 第一路） | PB15 / PB14 | IN1/IN2，当前闭环实际控制对象。 |
| 蠕动泵（L298N 第二路） | PB13 / PB12 | IN3/IN4，代码保留 `pump_set()`，当前闭环未使用。 |
| 泵/阀 PWM | TIM3 中断 | 软件 PWM，`timvalue` 0–100 与 CCR 比较；更新频率约 200 kHz，100 级 PWM 约 2 kHz。 |
| 调试串口 | USART2：PA2 TX / PA3 RX | 115200，8N1，接 USB-TTL / PC。 |
| ESP 串口 | USART3：PB10 TX / PB11 RX | 115200，8N1，与 ESP32 交叉连接。 |
| OLED | PB8 SCL / PB9 SDA | 软件 I2C；PB6/PB7 为 OLED 电源控制。 |
| 蜂鸣器 | PA1 | 推挽输出，低电平触发（`beep()` 拉低—延时—拉高）。 |
| LED | PB1 | 开漏输出，用于状态指示。 |
| 旋转编码器 | PA5 A / PA6 B / PA7 按键 | A/B 下降沿正交计数，按键下降沿触发，内部上拉。 |
| 心跳定时 | TIM4 | 100 ms 中断一次，`tim4_heartbeats` 累加。 |

> 接线以 `Serial.c`、`main.c` 顶部宏定义和上表为准。`main.c` 顶部同时保留了 `TXD PA_2`、`RXD PA_3` 与 `ESP_TX PB_10`、`ESP_RX PB_11` 的定义。

### 4.2 ESP32

| 功能 | 引脚/资源 |
|---|---|
| 与 STM32 串口 | UART2：GPIO17 TX / GPIO16 RX，115200 |
| Wi-Fi | 板载 |
| 手机端 | Blinker Wi-Fi 接入 |
| 配网 | 连接失败时开 AP：`ESP32_Config` / `12345678`，网页 `192.168.4.1` |

> 交叉连接：ESP32 GPIO17(TX) → STM32 PB11(RX)；ESP32 GPIO16(RX) → STM32 PB10(TX)；务必共地。

## 5. 工作逻辑

### 5.1 滴速检测

1. 红外对射/滴速传感器每滴一滴产生一个脉冲，接在 PA11。
2. `CountSensor_Init()` 配置 EXTI11 上升沿中断，并启动 TIM2（1 ms 计数）。
3. 每次中断：
   - 暂停 TIM2，读取累计的毫秒数作为两次滴落的时间间隔；
   - 小于 180 ms 的间隔视为抖动/干扰，沿用上一次有效值；
   - 否则写入 5 点滑动窗口，并刷新上一次有效值。
4. `CounterSensor_GetAvgValue()` 求窗口平均间隔。
5. `CountSensor_GetCountPerMinute()` 用 `60000 / 平均间隔(ms)` 换算每分钟滴数。
6. 主循环实时刷新 OLED 的 `Avg` 和 `Spd`，并在每次检测到滴落时短鸣提示。

### 5.2 阀门/泵控制

`L298N.c` 用两路 H 桥控制阀门和泵：

- `OFF`：两路方向都低，电机停止。
- `ZHENG`：IN1=1、IN2=0，正转。
- `FAN`：IN1=0、IN2=1，反转。

`valve_set(direct, speed)` 更新阀门方向和 PWM 占空比；`pump_set(direct, speed)` 更新泵。`L298N_SetCompare()` 只保存 CCR，真正的 PWM 在 `TIM3_IRQHandler()` 里用 `timvalue` 与 CCR 比较产生。

> 当前 `Tim3_Init()` 的 `PSC=72-1`、`ARR=5-1`，按 72 MHz 计算 TIM3 更新频率约 **200 kHz**，`timvalue` 0–100 循环得到约 **2 kHz** 的软件 PWM，响应速度已足够。闭环中 `valve_compare` 越大表示阀门越“拧紧”，越小越“放松”。

### 5.3 闭环调节

`User/main.c` 的 `infusion_loop()`：

1. **电脑调试口（USART2）命令**：
   - `@switch`：切换 `infusion_state`，关闭时 `valve_set(DIR_ZHENG, 100)`，打开时 `valve_set(DIR_OFF, 0)`，并回传 `State:n`。
   - `@set_speed`：提示输入 10–120，随后解析 `@<数字>` 并写入 `speed_required`。
   - `@acquire`：以 JSON 格式从 USART2 回传当前数据。
2. **ESP 口（USART3）命令**：见 5.4，支持 `@ESP` 握手、`@switch`、`@set_speed`、`@acquire`。
3. **本地编码器**：
   - 按下按键：切换 `infusion_state`，并清零 `miss_count` / `interrupt_time_overflow`；
   - 旋转：按增量修改 `speed_required`，并限制在 20–120。
4. **闭环（约 1 s 一次，`tim4_heartbeats >= 10`）**：
   - 实际滴速低于目标：`valve_compare -= 2`（放松阀门），下限 0；
   - 实际滴速高于目标：`valve_compare += 2`（拧紧阀门），上限 100；
   - 调用 `valve_set(DIR_ZHENG, valve_compare)` 更新阀门。
5. 每 1 s 统计一次“是否检测到滴落”，用于空液判断。

这是一个简单直观的 **比例式/步进式** 调节，不是严格 PID。泵/阀的机械死区、软管弹性、液位高度都会影响实际流量，换泵/换管后需要重新标定。

### 5.4 STM32 ↔ ESP32 串口协议（JSON）

STM32 端使用 `@` 开头、`\r\n` 结尾的文本命令，数据回传使用 **JSON**（每条以 `\n` 结束）。`cJSON` 生成的格式为：

```json
{"state":0,"speed":80,"requiredspeed":80}
```

| 方向 | 数据 | 作用 |
|---|---|---|
| ESP32 → STM32 | `@ESP\r\n` | 握手请求；STM32 回复 `STM\n`。 |
| ESP32 → STM32 | `@switch\r\n` | 切换输液开/关；STM32 随后回传 JSON。 |
| ESP32 → STM32 | `@set_speed\r\n` | 请求设置目标滴速；STM32 回复 `OK\n`。 |
| ESP32 → STM32 | `@<数字>\r\n` | 目标滴速（10–120）；STM32 回传 JSON，越界回复 `ERR\n`。 |
| ESP32 → STM32 | `@acquire\r\n` | 请求当前数据；STM32 回传 JSON。 |
| STM32 → ESP32 | `{"state":n,"speed":n,"requiredspeed":n}\n` | 开关状态、当前滴速、目标滴速。 |
| STM32 → ESP32 | `EMPTY\n` | 空液报警，ESP32 触发 Blinker 推送。 |
| STM32 → ESP32 | `RESET\n` | 报警被本地按键复位。 |

电脑调试口（USART2）同样支持 `@switch` / `@set_speed` / `@acquire`，区别是：`@switch` 回传文本 `State:n`，`@set_speed` 先回 `Please input the speed(10 - 120):`，成功回 `The speed is adjusted successfully:n`，`@acquire` 回传 JSON。

> ESP32 端解析时使用的键名严格为 `state`、`speed`、`requiredspeed`，字段名不要改。

### 5.5 空液报警

在闭环的每秒节拍中：

1. 若该秒检测到滴落，`interrupt_okay = 1`；否则 `interrupt_time_overflow++`。
2. 当 `(speed_required > count_per_min && valve_compare == 0)`（阀门已全开仍达不到目标）或 `interrupt_time_overflow >= 18`（约 18 s 无滴落）时，累计 `miss_count`。
3. 当 `miss_count >= 20` 或 `interrupt_time_overflow >= 18` 时：
   - `alarm_state = 1`，关闭阀门；
   - OLED 显示 `Empty!` / `Press R to reset`；
   - 蜂鸣器周期鸣叫，并通过 USART3 反复发送 `EMPTY\n`；
   - 进入等待状态，直到按下编码器按键。
4. 按键复位：`alarm_state = 0`、`infusion_state = 0`，复位阀门与计数，OLED 清屏，并发送 `RESET\n`。

### 5.6 ESP32：Wi-Fi、Blinker 与 Web 配网

`firmware/esp32/src/main.cpp` 的上电流程：

1. `Serial.begin(115200)` 打开调试串口。
2. `STM_Serial.begin(115200, SERIAL_8N1, 16, 17)` 打开与 STM32 的 UART2。
3. `STM_Init()` 循环发送 `@ESP`，等待 STM32 回复 `STM`。
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
7. `loop()` 中 `Blinker.run()`，并读取 STM32 串口；收到 `EMPTY` 且未报警时 `Blinker.push("输液已结束")`。
8. `STM_ReadData()` 发送 `@acquire`，用 `ArduinoJson` 解析返回的 JSON 并刷新控件。

> 安全提示：`main.cpp` 中的 Blinker 设备密钥已替换为 `YOUR_BLINKER_KEY`，避免把私人密钥上传到公开仓库。运行前请填入你自己的 Blinker 密钥。原文件备份在仓库外的 `大创项目整理_excluded/original/main.cpp.with_blinker_key`。

## 6. 构建与运行

### 6.1 STM32（Keil MDK）

1. 安装 Keil MDK5、STM32F1xx Device Family Pack、ST-Link 驱动。
2. 打开 `firmware/stm32/Project.uvprojx`。
3. 编译（Build），用 ST-Link 下载。
4. USART2（PA2/PA3）接 USB-TTL 观察调试输出；USART3（PB10/PB11）接 ESP32，115200 8N1。
5. 上电后 OLED 显示平均间隔、每分钟滴数、开关状态与目标滴速；蜂鸣器在每次滴落时短鸣。
6. 旋转编码器调速，按下按键开关输液；空液报警时按下编码器按键复位。

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

1. **PB11 初始化残留**：`CountSensor_Init()` 把 GPIOB Pin 11 配成上拉输入，但 EXTI 映射与 ISR 使用的都是 PA11；而 PB11 现在是 **USART3_RX**。建议删掉这段 GPIOB 初始化，或明确统一传感器端口，避免和 ESP 串口混淆。
2. **串口接收状态机较脆弱**：`USART2_IRQHandler` / `USART3_IRQHandler` 只识别 `@...\r\n`；如果状态 2 收到的不是 `\n`，状态机会停在状态 2 无法恢复。缓冲区（100 字节）也没有长度保护，建议加边界检查与超时复位。
3. **除零/初值问题**：`CountSensor_GetCountPerMinute()` 直接 `60000 / avg`。上电初期滑动窗口为 0 时，Cortex-M3 硬件除法结果不可靠（通常返回 0），仍建议加 `if (avg == 0) return 0;`。
4. **阻塞等待**：ESP32 的 `STM_Init()`、`STM_ReadData()`、`STM_Setspeed()`、`STM_Switch()` 都是无限 `while(1)` 等待串口，没有超时；STM32 不应答时会卡死。建议加超时和重试。
5. **滴速范围不完全一致**：串口 `set_speed` 接受 10–120，编码器调速限制在 20–120。若不希望现场调到 10–19，请统一范围。
6. **泵通道未参与闭环**：当前只调用 `valve_set()`，`pump_compare` 固定，泵输出为 0。若硬件使用蠕动泵调速，请改用 `pump_set()` 或同时控制泵阀。
7. **cJSON 依赖堆内存**：`cJSON_CreateObject()` / `cJSON_PrintUnformatted()` 使用 `malloc`，`JSONData_buffer` 固定 256 字节。若数据字段增多，请同步增大缓冲区并关注堆碎片/内存是否足够。
8. **无液位检测的语义**：空液判断基于“长时间无滴落/阀门全开仍不达标”，属于间接推断，不是液位传感器；软管折弯、传感器脏污也可能误报。
9. **源码编码混用**：STM32 用户代码多为 UTF-8，部分标准库文件为 GBK；若中文注释乱码，请在编辑器中切换 GBK/GB18030 或 UTF-8。
10. **机械/流体标定**：滴速与阀门开度、软管直径、液位高度强相关，换泵/换管后必须重新标定 `valve_compare` 与 `count_per_min` 的关系。

## 8. 资料与许可

- `docs/references` 中的论文/专利仅用于学习调研，版权归原作者/权利人所有。
- `firmware/esp32/lib/Blinker-0.3.10230510` 为 Blinker 官方库，遵循其原始许可证。
- `firmware/stm32/System/cJSON.c/.h` 来自 Dave Gamble 的 cJSON 项目，遵循其原始 MIT 许可证。
- STM32 标准外设库版权归 STMicroelectronics。
- 本项目的应用层代码用于学习、交流和后续大创/竞赛参考，请保留原作者与资料出处。

---

## English Summary

This repository contains the **Smart Infusion Guardian System**, a 2024 Hefei University of Technology university-level innovation project that passed acceptance and won the **Provincial First Prize** in the “Challenge Cup” team track.

The system uses an **STM32F103C8T6** as the real-time controller and an **ESP32** as the Wi-Fi/Blinker gateway. The STM32 firmware now includes **cJSON**, so the two MCUs exchange structured JSON data.

- The STM32 reads an infrared drip sensor on PA11 (EXTI rising edge), measures the interval between drops with TIM2, applies a 5-sample sliding window, and computes drops per minute.
- It drives a valve through an L298N dual H-bridge with software PWM (TIM3, ~2 kHz at 100 steps), and runs a simple 1-second proportional/step speed update. The second H-bridge channel (pump) is kept as a spare.
- It displays data on an OLED (PB8/PB9 software I2C), beeps on each detected drop, and reads a rotary encoder (PA5/PA6 + PA7 button) for local speed adjustment and on/off control.
- It implements **empty-bag detection**: if no drop is seen for ~18 s or the fully-open valve still cannot reach the target, it closes the valve, shows `Empty!`, raises the buzzer, sends `EMPTY\n` to the ESP32, and waits for an encoder-button reset (`RESET\n`).
- It talks to the PC over **USART2 (PA2/PA3)** and to the ESP32 over **USART3 (PB10/PB11)**, both at 115200 8N1, using `@command\r\n` text commands.
- JSON responses have the form `{"state":0,"speed":80,"requiredspeed":80}\n`, parsed by the ESP32 with ArduinoJson.
- The ESP32 connects to Wi-Fi, runs the Blinker app widgets, provides a Web AP configuration page (`ESP32_Config` / `12345678` / `192.168.4.1`), performs the `@ESP` → `STM` handshake, and bridges UART2 (GPIO16/17, 115200) to the STM32.
- The PCB, schematic, Gerber, design mind map, and reference papers are under `docs/`.

Key folders:

- `firmware/stm32` — Keil MDK project: `User/main.c`, `Hardware/` (drip sensor, L298N, OLED, UART, timer, buzzer, encoder), `System/` (delay, cJSON), `Start/`, `Library/`.
- `firmware/esp32` — PlatformIO project: `src/main.cpp`, `platformio.ini`, local Blinker library.
- `docs/references` — infusion sensor/monitoring papers and patents.
- `docs/design` — system mind map.
- `docs/pcb/V1.0` — schematic, Gerber, board parameters.

Important notes:

- The Blinker device key in the original source has been replaced with `YOUR_BLINKER_KEY`; fill in your own key before running.
- `CountSensor_Init()` still configures GPIOB Pin 11 (now USART3_RX) even though the EXTI line and ISR use PA11; remove or unify this.
- The UART receive state machines only accept `@...\r\n`, can get stuck in state 2, and do not bound the 100-byte buffers; add guards.
- `count_per_min` divides by the average interval without a zero check; add a guard.
- ESP32 serial waits are blocking without timeouts; add timeouts/retries.
- `set_speed` accepts 10–120 while encoder adjustment clamps to 20–120; unify if desired.
- The pump channel is not used in the current closed loop; only the valve is driven.
- cJSON uses heap allocation; keep the 256-byte JSON buffer and heap usage in mind.
- Empty-bag detection is inferred from “no drip / valve fully open”, not from a real level sensor.
- Source encodings are mixed (UTF-8 user code, some GBK standard-library files); switch encoding if Chinese comments are garbled.

The application code is shared for learning and future competition reference. Please keep the original author and data-source credits.
