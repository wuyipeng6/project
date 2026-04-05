基于 STM32 + FreeRTOS + LwIP 的工业级物联网网关

1. 项目简介
本项目是一款基于 STM32F407ZGT6 开发的工业级物联网网关原型。系统集成 FreeRTOS 实时操作系统，通过 LAN8720A 物理层芯片配合 LwIP 协议栈实现以太网通信。
该网关实现了传感器数据采集（SHT20）、执行器远程控制（SG90）、本地状态显示（TFTLCD）、及云端（MQTT）双向通信，并具备 OTA 远程固件升级能力。
开发平台vscode+EIDE插件

2. 硬件资源
主控：STM32F407ZGT6 (Cortex-M4, 168MHz, 1MB Flash, 192KB RAM)
网络模块：LAN8720A (RMII 接口)
显示屏：2.8寸 TFT LCD (FSMC 接口/SPI 接口)
传感器：SHT20 (I2C 接口，温湿度采集)
执行器：SG90 舵机 (PWM 控制，模拟远程开关/阀门)
调试接口：USART1 (Logging), SWD

3. 软件架构 (Software Stack)
RTOS: FreeRTOS (任务优先级管理、信号量、队列、互斥锁)
网络栈: LwIP (支持 TCP/IP, UDP, DHCP, HTTP, ICMP)
应用层协议: MQTT , HTTP (用于OTA升级)
驱动层: 基于 STM32 HAL 库 

4. 核心功能与实现方案

4.1 多任务并行管理 (FreeRTOS)
系统划分为以下核心任务，确保实时性与稳定性：
Task_Net_Main (最高优先级)：负责 LwIP 协议栈内核处理。（由LWIP源码提供）
Task_MQTT_Client：处理 MQTT 连接维护、订阅下发命令、心跳包发送。
Task_Sensor_Collect：通过 I2C 读取 SHT20 数据，利用信号量同步采集周期。
Task_UI_Display：刷新 2.8 寸 LCD 屏，显示 IP 地址、传感器数值及连接状态。
Task_Actuator_Ctrl：响应云端指令，通过 消息队列 接收角度指令并控制 SG90。
Task_IWDG_Feed：负责喂狗，程序卡死则复位

4.2 网络通信方案 (LwIP + MQTT)
双向通信：
上行：SHT20 温湿度数据定时打包成 JSON 格式上传至阿里云/腾讯云。
下行：云端发布指令，网关解析 JSON 后控制 SG90 舵机旋转（模拟工业阀门启闭）。云端发布更新指令，APP自动从HTTP服务器更新固件

4.3 稳定性设计 (亮点)
MQTT 断线重连机制：设计状态机处理网络抖动，当检测到 TCP 断开时，自动释放资源并尝试指数级退避重连。
硬看门狗 (IWDG)：在独立任务中喂狗，确保系统在极端死机情况下自动复位。

4.4 OTA 远程升级
实现 Bootloader + App 结构。
通过 MQTT 接收升级指令，利用 HTTP 下载固件至 Flash 特定区域。
FLASH分区设计：
采用SPI FLASH+内部FLASH，并存，
SPI FLASH用来存放download区域和factory区域固件
内部FLASH用来存放Bootloader和APP区域固件。
采用FAL库对FLASH分区，采用sfud库驱动SPI FLASH
通过 CRC 校验 确保固件完整性，



