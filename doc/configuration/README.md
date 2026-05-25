# 配置与接口

本文档汇总工程中最重要的可配置项和对外接口，包括引脚分配、EEPROM 布局、AT 指令、Web 配置入口和运行时参数边界。

如果需要把引脚、按键、SD 卡槽、UART 接口与板卡外观对应起来，建议结合 [硬件设计资料](../hardware/README.md) 一起阅读。

## 1. 硬件引脚分配

硬件图示入口：原理图、PCB 图、3D 仿真图和实物图见 [硬件设计资料](../hardware/README.md)。

| 功能 | 引脚 | 说明 |
| --- | --- | --- |
| BOOT 按键 | GPIO0 | 短按切模式，长按恢复默认 |
| 配网触发 | GPIO42 | 上电时拉低可触发配网入口 |
| RGB LED | GPIO48 | 状态显示 |
| UART2 RX / TX | GPIO16 / GPIO17 | 主透传串口 |
| UART1 RX / TX | GPIO19 / GPIO20 | 独立采集/调试串口 |
| 电源控制 | GPIO5 | 电源控制输出 |
| CPU 复位控制 | GPIO4 | 复位输出 |
| SD CS / MOSI / MISO / SCK | GPIO10 / GPIO11 / GPIO13 / GPIO12 | SPI 接口 |
| SD 电源控制 | GPIO8 | SD 供电开关 |
| 电池 ADC | GPIO1 | 电池电压采样 |

## 2. EEPROM 布局

EEPROM 总大小为 512 字节，当前主要占用如下：

| 地址范围 | 名称 | 说明 |
| --- | --- | --- |
| 0 | EEPROM_MODE_ADDR | 当前运行模式 |
| 10-41 | EEPROM_CLIENT_ID_ADDR | 客户端 ID 字符串 |
| 46-49 | EEPROM_UART2_BAUD_ADDR | UART2 波特率 |
| 50-53 | EEPROM_UART1_BAUD_ADDR | UART1 波特率 |
| 54 | EEPROM_LOWPOWER_TIMEOUT | 预留/历史字段 |
| 60 | EEPROM_LOGTIME_ADDR | 日志时间戳开关 |
| 61 | EEPROM_DEBUGMODE_ADDR | 调试模式开关 |
| 66 | EEPROM_WIFI_MAGIC_ADDR | WiFi 配置有效标记 |
| 70-102 | EEPROM_AP_SSID_ADDR | 服务器模式 AP SSID |
| 103-167 | EEPROM_AP_PASS_ADDR | 服务器模式 AP 密码 |
| 168-200 | EEPROM_STA_SSID_ADDR | 客户端模式 SSID |
| 201-265 | EEPROM_STA_PASS_ADDR | 客户端模式密码 |
| 266-298 | EEPROM_PORTAL_SSID_ADDR | 配网热点 SSID |
| 299-363 | EEPROM_PORTAL_PASS_ADDR | 配网热点密码 |

说明：WiFi 字段写入前会做合法性校验；若 EEPROM 中内容非法或未初始化，则系统自动生成设备唯一默认值并回写。

## 3. 默认配置生成策略

工程会基于芯片 eFuse MAC 生成默认标识：

- client_id: ESP32_CLIENT_<token>
- ap_ssid: ESP32_UART_<token>
- ap_password: ESP32#<token>!
- wifimanager_ssid: ESP32_CFG_<token>
- wifimanager_password: 与 ap_password 保持一致

这样做的目的，是避免硬编码统一凭据和多个设备同名冲突。

## 4. 运行时配置入口

### 4.1 按键配置

- 短按：切换客户端/服务器模式
- 长按：恢复默认配置并重启

### 4.2 USB AT 指令

AT 指令通过 USB CDC 输入，必须以 AT+ 前缀触发管理逻辑。

| 指令 | 说明 |
| --- | --- |
| AT+HELP / AT+? | 显示帮助 |
| AT+SWITCH | 切换工作模式 |
| AT+RESET | 恢复默认配置 |
| AT+STATUS | 查看系统状态 |
| AT+CPU | 查看 CPU 和内存信息 |
| AT+CLIENTS | 查看服务器模式客户端列表 |
| AT+SELECT | 查看可选客户端 |
| AT+SELECT <id> | 选择透传目标客户端，-1 表示取消 |
| AT+LOGNAME <name> | 设置日志文件名前缀 |
| AT+LOGTIME=ON / OFF | 开启或关闭日志时间戳 |
| AT+BAUD <rate> | 设置 UART2 波特率 |
| AT+BAUD1 <rate> | 设置 UART1 波特率 |
| AT+DEBUG=ON / OFF | 开关调试模式 |
| AT+RESTART | 软件重启 |
| AT+CONFIG | 进入非阻塞配网模式 |
| AT+EXITCONFIG | 退出配网模式 |
| AT+POWER=ON / OFF / TRIGGER | 电源控制相关操作 |
| AT+RESET=CPU | 触发 CPU 复位 |

说明：AT+RAW 已被安全策略禁用，不再允许原始旁路透传模式。

### 4.3 Web 配置页

Web 配置页位于 /config，保存入口为 /saveconfig。可修改：

- 运行模式
- 客户端 ID
- UART2 波特率
- 调试模式
- 日志时间戳
- AP SSID / 密码
- 客户端 WiFi SSID / 密码
- 配网热点 SSID / 密码

安全规则：

- SSID 最长 32 字节
- 密码长度必须在 8-63 之间
- 输入字符必须是安全可打印字符
- 敏感字段默认不回显，留空表示保持原值

### 4.4 WiFiManager 配网入口

以下两种情况会进入配网模式：

- 客户端模式下没有有效的 WiFi 配置
- 上电时检测到 CONFIG_MODE_PIN 被拉低

配网成功后，获取到的 SSID 和密码会先做安全校验，再写入 EEPROM。

## 5. 网络参数

| 参数 | 当前值 | 说明 |
| --- | --- | --- |
| server_ip | 192.168.1.1 | 客户端模式默认连接目标 |
| server_port | 8080 | 客户端连接端口 |
| server_listen_port | 8080 | 服务器模式监听端口 |
| MAX_CLIENTS | 5 | 服务器模式最大客户端数 |

注意：当前 server_ip 仍是固定常量，适合与本项目服务器模式配套使用。如需对接第三方服务器，建议后续将其加入 EEPROM 或 Web 配置项。

## 6. 串口参数

| 参数 | 默认值 | 范围 |
| --- | --- | --- |
| UART2 波特率 | 115200 | 9600 - 921600 |
| UART1 波特率 | 115200 | 9600 - 921600 |
| USB AT 命令缓冲 | 64 字节 | 超出后计为失败 |
| 通用 UART/String 缓冲 | 1024 字节 | 主要用于本地命令与显示缓冲控制 |

## 7. 日志相关配置

| 配置 | 说明 |
| --- | --- |
| logFileName | 日志文件名前缀 |
| logWithTimestamp | 是否给每条日志加时间戳 |
| logToSD | 是否启用 SD 落盘 |
| SD_WRITE_INTERVAL_MS | 异步批量写盘间隔，当前为 1000 ms |
| SD_WRITE_BATCH_SIZE | 每批次写入条数，当前为 32 |

## 8. LED 状态语义

| 状态 | 说明 |
| --- | --- |
| LED_FAST_BLINK | 客户端模式未联网 |
| LED_SLOW_BLINK | 服务器模式待连接 |
| LED_SOLID_GREEN | 连接正常 |
| LED_SOLID_YELLOW | 部分连接或配网状态 |
| LED_SD_ERROR | SD 卡异常 |
| LED_LOW_BATTERY | 电池低电压告警 |

## 9. 建议阅读下一篇

继续阅读 [构建、烧录与运维](../operation/README.md)，了解实际编译、烧录、Web 路由和日志目录结构。