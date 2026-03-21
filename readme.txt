基于 STM32 + FreeRTOS + LwIP 的工业级物联网网关
1. 项目简介

本项目是一款基于 STM32F407ZGT6 开发的工业级物联网网关原型。系统集成 FreeRTOS 实时操作系统，通过 LAN8720A 物理层芯片配合 LwIP 协议栈实现以太网通信。

该网关实现了传感器数据采集（SHT20）、执行器远程控制（SG90）、本地状态显示（TFTLCD）、Web 端参数配置以及云端（MQTT）双向通信，并具备 OTA 远程固件升级 能力。

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

应用层协议: MQTT (基于 Paho MQTT 库), HTTP (用于 Web Server 页面)

驱动层: 基于 STM32 HAL 库 + DMA 优化

4. 核心功能与实现方案
4.1 多任务并行管理 (FreeRTOS)

系统划分为以下核心任务，确保实时性与稳定性：

Task_Net_Main (最高优先级)：负责 LwIP 协议栈内核处理。

Task_MQTT_Client：处理 MQTT 连接维护、订阅下发命令、心跳包发送。

Task_Sensor_Collect：通过 I2C 读取 SHT20 数据，利用信号量同步采集周期。

Task_UI_Display：刷新 2.8 寸 LCD 屏，显示 IP 地址、传感器数值及连接状态。

Task_Actuator_Ctrl：响应云端指令，通过 消息队列 接收角度指令并控制 SG90。

4.2 网络通信方案 (LwIP + MQTT)

Web Server 配置：利用 LwIP 实现轻量级 HTTP Server。用户可通过浏览器访问网关 IP，在网页端配置网关的静态 IP 或云平台参数。

双向通信：

上行：SHT20 温湿度数据定时打包成 JSON 格式上传至阿里云/腾讯云。

下行：云端发布指令，网关解析 JSON 后控制 SG90 舵机旋转（模拟工业阀门启闭）。

4.3 稳定性设计 (亮点)

MQTT 断线重连机制：设计状态机处理网络抖动，当检测到 TCP 断开时，自动释放资源并尝试指数级退避重连。

内存监控：针对 STM32 RAM 资源，优化 LwIP PBUF 内存池配置，防止长时间运行下的内存泄漏。

硬看门狗 (IWDG)：在独立任务中喂狗，确保系统在极端死机情况下自动复位。

4.4 OTA 远程升级

实现 Bootloader + App 结构。

通过 MQTT 接收升级包信息，利用 HTTP 下载固件至 Flash 特定区域。

通过 CRC 校验 确保固件完整性，实现断电不丢失的平滑升级。

5. 项目目录结构

6. 技术难点及解决过程 (面试重点)

LwIP 与 FreeRTOS 的整合：通过配置 tcpip_thread 确保网络收发在独立线程，解决裸机协议栈在多任务环境下的竞争问题。

LCD 刷新与任务切换的卡顿优化：将 LCD 底层驱动改为 FSMC (8080接口) 驱动，大幅降低 CPU 占用率，确保在频繁网络通信时 UI 依然流畅。

I2C 传感器阻塞问题：由于 I2C 是同步协议，为防止读取 SHT20 时阻塞其他任务，引入了 FreeRTOS 的超时机制 (Timeout) 并在异常时自动复位 I2C 总线。

7. 如何运行

使用 Keil MDK打开工程。

连接 LAN8720A 模块至 STM32 的 RMII 对应引脚。

修改 mqtt_config.h 中的云平台鉴权信息。

编译并下载固件，通过串口助手查看运行日志。

在 LCD 屏查看网关获取的 IP 地址，并在浏览器输入该 IP 进入配置页面。

