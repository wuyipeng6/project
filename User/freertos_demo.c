/**
 ****************************************************************************************************
 * @file        freertos_demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-01-11
 * @brief       lwIP+FreeRTOS操作系统移植实验
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

#include "freertos_demo.h"
#include "./BSP/LED/led.h"
#include "./BSP/LCD/lcd.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "lwip_comm.h"
#include "lwipopts.h"
#include "FreeRTOS.h"
#include "task.h"

/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO 5			 /* 任务优先级 */
#define START_STK_SIZE 128			 /* 任务堆栈大小 */
TaskHandle_t StartTask_Handler;		 /* 任务句柄 */
void start_task(void *pvParameters); /* 任务函数 */

/* LWIP_DEMO 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define LWIP_DMEO_TASK_PRIO 11			 /* 任务优先级 */
#define LWIP_DMEO_STK_SIZE 1024			 /* 任务堆栈大小 */
TaskHandle_t LWIP_Task_Handler;			 /* 任务句柄 */
void lwip_demo_task(void *pvParameters); /* 任务函数 */

/* LED_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define LED_TASK_PRIO 10		   /* 任务优先级 */
#define LED_STK_SIZE 128		   /* 任务堆栈大小 */
TaskHandle_t LEDTask_Handler;	   /* 任务句柄 */
void led_task(void *pvParameters); /* 任务函数 */
/******************************************************************************************************/

/**
 * @breif       加载UI
 * @param       mode :  bit0:0,不加载;1,加载前半部分UI
 *                      bit1:0,不加载;1,加载后半部分UI
 * @retval      无
 */
void lwip_test_ui(uint8_t mode)
{
	uint8_t speed;
	uint8_t buf[30];

	if (mode & 1 << 0)
	{
		lcd_fill(5, 30, lcddev.width, 110, WHITE);
		lcd_show_string(5, 30, 200, 16, 16, "STM32", RED);
		lcd_show_string(5, 50, 200, 16, 16, "lwIP ping Test", RED);
		lcd_show_string(5, 70, 200, 16, 16, "ATOM@ALIENTEK", RED);
	}

	if (mode & 1 << 1)
	{
		lcd_fill(5, 110, lcddev.width, lcddev.height, WHITE);
		lcd_show_string(5, 110, 200, 16, 16, "lwIP Init Successed", BLUE);

		if (g_lwipdev.dhcpstatus == 2)
		{
			sprintf((char *)buf, "DHCP IP:%d.%d.%d.%d", g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]); /* 显示动态IP地址 */
		}
		else
		{
			sprintf((char *)buf, "Static IP:%d.%d.%d.%d", g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]); /* 打印静态IP地址 */
		}

		lcd_show_string(5, 130, 200, 16, 16, (char *)buf, BLUE);

		speed = ethernet_chip_get_speed(); /* 得到网速 */

		if (speed)
		{
			lcd_show_string(5, 150, 200, 16, 16, "Ethernet Speed:100M", BLUE);
		}
		else
		{
			lcd_show_string(5, 150, 200, 16, 16, "Ethernet Speed:10M", BLUE);
		}
	}
}

/**
 * @breif       freertos_demo
 * @param       无
 * @retval      无
 */
void freertos_demo(void)
{
	/* start_task任务 */
	xTaskCreate((TaskFunction_t)start_task,
				(const char *)"start_task",
				(uint16_t)START_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)START_TASK_PRIO,
				(TaskHandle_t *)&StartTask_Handler);

	vTaskStartScheduler(); /* 开启任务调度 */
}

/**
 * @brief       start_task
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void start_task(void *pvParameters)
{
	pvParameters = pvParameters;

	g_lwipdev.lwip_display_fn = lwip_test_ui;

	g_lwipdev.lwip_display_fn = lwip_test_ui;

	lwip_test_ui(1); /* 加载后前部分UI */

	while (lwip_comm_init() != 0)
	{
		lcd_show_string(30, 110, 200, 16, 16, "lwIP Init failed!!", RED);
		delay_ms(500);
		lcd_fill(30, 50, 200 + 30, 50 + 16, WHITE);
		lcd_show_string(30, 110, 200, 16, 16, "Retrying...       ", RED);
		delay_ms(500);
		LED1_TOGGLE();
	}

	while (!ethernet_read_phy(PHY_SR)) /* 检查MCU与PHY芯片是否通信成功 */
	{
		printf("MCU与PHY芯片通信失败，请检查电路或者源码！！！！\r\n");
	}
	#if LWIP_DHCP															  /* 使用动态IP */
	while ((g_lwipdev.dhcpstatus != 2) && (g_lwipdev.dhcpstatus != 0XFF)) /* 等待DHCP获取成功*/
	{
		vTaskDelay(5);
	}
	#else 
	{
		lwip_test_ui(2); /* 加载后前部分UI */
	}
	#endif
	taskENTER_CRITICAL(); /* 进入临界区 */

	/* 创建lwIP任务 */
	xTaskCreate((TaskFunction_t)lwip_demo_task,
				(const char *)"lwip_demo_task",
				(uint16_t)LWIP_DMEO_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)LWIP_DMEO_TASK_PRIO,
				(TaskHandle_t *)&LWIP_Task_Handler);

	/* LED测试任务 */
	xTaskCreate((TaskFunction_t)led_task,
				(const char *)"led_task",
				(uint16_t)LED_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)LED_TASK_PRIO,
				(TaskHandle_t *)&LEDTask_Handler);

	vTaskDelete(StartTask_Handler); /* 删除开始任务 */
	taskEXIT_CRITICAL();			/* 退出临界区 */
}

/**
 * @brief       lwIP运行例程
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void lwip_demo_task(void *pvParameters)
{
	pvParameters = pvParameters;

	uint8_t t = 0;

	while (1)
	{
		t++;

		if ((t % 40) == 0)
		{
			LED0_TOGGLE(); /* 翻转一次LED0 */
		}

		vTaskDelay(5);
	}
}

/**
 * @brief       系统再运行
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void led_task(void *pvParameters)
{
	pvParameters = pvParameters;

	while (1)
	{
		LED1_TOGGLE();
		vTaskDelay(1000);
	}
}
