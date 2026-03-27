/**
 ****************************************************************************************************
 * @file        freertos_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-01-11
 * @brief       lwIP+FreeRTOS操作系统移植 实验
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 探索者 F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */
 
#ifndef __FREERTOS_DEMO_H
#define __FREERTOS_DEMO_H

#include "FreeRTOS.h"
#include "task.h"

typedef struct
{
	float temperature;
	float humidity;
	uint8_t ip[4];
	uint8_t mqtt_connected;
} app_runtime_data_t;


void freertos_demo(void);   /* 创建lwIP的任务函数 */

/* 应用运行态数据读写接口（内部已做线程保护） */
void app_data_set_sensor(float temperature, float humidity);
void app_data_set_ip(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3);
void app_data_set_mqtt_connected(uint8_t connected);
void app_data_get_snapshot(app_runtime_data_t *out);

/* 执行器命令接口（角度: 0~180） */
BaseType_t app_actuator_send_angle(uint16_t angle, TickType_t ticks_to_wait);

#endif
