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
#include "./BSP/TIMER/btim.h"
#include "./BSP/SG90/sg90.h"
#include "./BSP/SHT20/driver_sht2x_basic.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "lwip_comm.h"
#include "lwip_demo.h"
#include "lwipopts.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "timers.h"
#include "./SYSTEM/sys/sys.h"

/******************************************************************************************************/
/*FreeRTOS配置*/

/* START_TASK 任务 配置
 * 包括: 任务句柄 任务优先级 堆栈大小 创建任务
 */
#define START_TASK_PRIO 5			 /* 任务优先级 */
#define START_STK_SIZE 128			 /* 任务堆栈大小 */
TaskHandle_t StartTask_Handler;		 /* 任务句柄 */
void start_task(void *pvParameters); /* 任务函数 */

/* Task_MQTT_Client 任务 配置 */
#define MQTT_TASK_PRIO 12
#define MQTT_STK_SIZE 2048
TaskHandle_t MQTTTask_Handler;
void mqtt_client_task(void *pvParameters);

/* Task_Sensor_Collect 任务 配置 */
#define SENSOR_TASK_PRIO 10
#define SENSOR_STK_SIZE 256
TaskHandle_t SensorTask_Handler;
void sensor_collect_task(void *pvParameters);

/* Task_UI_Display 任务 配置 */
#define UI_TASK_PRIO 9
#define UI_STK_SIZE 384
TaskHandle_t UITask_Handler;
void ui_display_task(void *pvParameters);

/* Task_Actuator_Ctrl 任务 配置 */
#define ACTUATOR_TASK_PRIO 11
#define ACTUATOR_STK_SIZE 256
TaskHandle_t ActuatorTask_Handler;
void actuator_ctrl_task(void *pvParameters);

/* Task_IWDG_Feed 任务 配置 */
#define IWDG_TASK_PRIO 2
#define IWDG_STK_SIZE 128
TaskHandle_t IWDGTask_Handler;
void iwdg_feed_task(void *pvParameters);

/* 应用内核对象 */
static QueueHandle_t g_actuator_queue = NULL;
static SemaphoreHandle_t g_sensor_period_sem = NULL;
static SemaphoreHandle_t g_app_data_mutex = NULL;
static TimerHandle_t g_sensor_timer = NULL;

static app_runtime_data_t g_app_data = {0};
static IWDG_HandleTypeDef g_iwdg_handle;

static void sensor_period_timer_cb(TimerHandle_t xTimer);
static uint8_t iwdg_init_3s(void);
/******************************************************************************************************/

void app_data_set_sensor(float temperature, float humidity)
{
	if (g_app_data_mutex != NULL)
	{
		xSemaphoreTake(g_app_data_mutex, portMAX_DELAY);
		g_app_data.temperature = temperature;
		g_app_data.humidity = humidity;
		xSemaphoreGive(g_app_data_mutex);
	}
}

void app_data_set_ip(uint8_t ip0, uint8_t ip1, uint8_t ip2, uint8_t ip3)
{
	if (g_app_data_mutex != NULL)
	{
		xSemaphoreTake(g_app_data_mutex, portMAX_DELAY);
		g_app_data.ip[0] = ip0;
		g_app_data.ip[1] = ip1;
		g_app_data.ip[2] = ip2;
		g_app_data.ip[3] = ip3;
		xSemaphoreGive(g_app_data_mutex);
	}
}

void app_data_set_mqtt_connected(uint8_t connected)
{
	if (g_app_data_mutex != NULL)
	{
		xSemaphoreTake(g_app_data_mutex, portMAX_DELAY);
		g_app_data.mqtt_connected = connected;
		xSemaphoreGive(g_app_data_mutex);
	}
}

void app_data_get_snapshot(app_runtime_data_t *out)
{
	if ((out == NULL) || (g_app_data_mutex == NULL))
	{
		return;
	}

	xSemaphoreTake(g_app_data_mutex, portMAX_DELAY);
	*out = g_app_data;
	xSemaphoreGive(g_app_data_mutex);
}

BaseType_t app_actuator_send_angle(uint16_t angle, TickType_t ticks_to_wait)
{
	if (g_actuator_queue == NULL)
	{
		return pdFAIL;
	}

	if (angle > 180U)
	{
		angle = 180U;
	}

	return xQueueSend(g_actuator_queue, &angle, ticks_to_wait);
}

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

	g_actuator_queue = xQueueCreate(8, sizeof(uint16_t));
	g_sensor_period_sem = xSemaphoreCreateBinary();
	g_app_data_mutex = xSemaphoreCreateMutex();
	// 创建一个周期为1s的定时器，定时器回调函数中释放g_sensor_period_sem信号量，触发传感器数据采集
	g_sensor_timer = xTimerCreate("sensor_tmr", pdMS_TO_TICKS(1000), pdTRUE, NULL, sensor_period_timer_cb);
	if ((g_actuator_queue == NULL) || (g_sensor_period_sem == NULL) || (g_app_data_mutex == NULL) || (g_sensor_timer == NULL))
	{
		printf("APP RTOS objects create failed.\r\n");
		while (1)
		{
			LED1_TOGGLE();
			vTaskDelay(pdMS_TO_TICKS(200));
		}
	}

	app_data_set_ip(g_lwipdev.ip[0], g_lwipdev.ip[1], g_lwipdev.ip[2], g_lwipdev.ip[3]);
	app_data_set_mqtt_connected(0);

	if (iwdg_init_3s() != 0)
	{
		printf("IWDG init failed.\r\n");
		while (1)
		{
			LED1_TOGGLE();
			vTaskDelay(pdMS_TO_TICKS(100));
		}
	}

	xTimerStart(g_sensor_timer, 0);

	taskENTER_CRITICAL(); /* 进入临界区 */

	/* 创建 Task_MQTT_Client */
	xTaskCreate((TaskFunction_t)mqtt_client_task,
				(const char *)"Task_MQTT_Client",
				(uint16_t)MQTT_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)MQTT_TASK_PRIO,
				(TaskHandle_t *)&MQTTTask_Handler);

	/* 创建 Task_Sensor_Collect */
	xTaskCreate((TaskFunction_t)sensor_collect_task,
				(const char *)"Task_Sensor_Collect",
				(uint16_t)SENSOR_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)SENSOR_TASK_PRIO,
				(TaskHandle_t *)&SensorTask_Handler);

	/* 创建 Task_UI_Display */
	xTaskCreate((TaskFunction_t)ui_display_task,
				(const char *)"Task_UI_Display",
				(uint16_t)UI_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)UI_TASK_PRIO,
				(TaskHandle_t *)&UITask_Handler);

	/* 创建 Task_Actuator_Ctrl */
	xTaskCreate((TaskFunction_t)actuator_ctrl_task,
				(const char *)"Task_Actuator_Ctrl",
				(uint16_t)ACTUATOR_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)ACTUATOR_TASK_PRIO,
				(TaskHandle_t *)&ActuatorTask_Handler);

	/* 创建 Task_IWDG_Feed */
	xTaskCreate((TaskFunction_t)iwdg_feed_task,
				(const char *)"Task_IWDG_Feed",
				(uint16_t)IWDG_STK_SIZE,
				(void *)NULL,
				(UBaseType_t)IWDG_TASK_PRIO,
				(TaskHandle_t *)&IWDGTask_Handler);

	taskEXIT_CRITICAL();			/* 退出临界区 */
	vTaskDelete(StartTask_Handler); /* 删除开始任务 */
}

/**
 * @brief       lwIP运行例程
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void mqtt_client_task(void *pvParameters)
{
	pvParameters = pvParameters;

	lwip_demo();

	while (1)
	{
		vTaskDelay(5);
	}
}

/**
 * @brief       Task_Sensor_Collect
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void sensor_collect_task(void *pvParameters)
{
	float temperature;
	float humidity;

	pvParameters = pvParameters;

	while (sht2x_basic_init() != 0)
	{
		printf("SHT20 init failed, retry...\r\n");
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	printf("SHT20 init success.\r\n");

	while (1)
	{
		xSemaphoreTake(g_sensor_period_sem, portMAX_DELAY);

		if (sht2x_basic_read(&temperature, &humidity) == 0)
		{
			app_data_set_sensor(temperature, humidity);
			printf("SHT20 T=%.2fC RH=%.2f%%\r\n", temperature, humidity);
		}
		else
		{
			printf("SHT20 read failed.\r\n");
		}
	}
}

/**
 * @brief       Task_UI_Display
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void ui_display_task(void *pvParameters)
{
	app_runtime_data_t snapshot;
	uint8_t buf[48];

	pvParameters = pvParameters;
	lcd_fill(5, 210, lcddev.width, lcddev.height, WHITE);

	while (1)
	{
		app_data_get_snapshot(&snapshot);

		sprintf((char *)buf, "IP:%d.%d.%d.%d", snapshot.ip[0], snapshot.ip[1], snapshot.ip[2], snapshot.ip[3]);
		lcd_show_string(5, 210, 220, 16, 16, (char *)buf, BLUE);

		sprintf((char *)buf, "T:%.2fC RH:%.2f%%", snapshot.temperature, snapshot.humidity);
		lcd_show_string(5, 230, 220, 16, 16, (char *)buf, BLUE);

		if (snapshot.mqtt_connected && (g_lwipdev.link_status == LWIP_LINK_ON))
		{
			lcd_show_string(5, 250, 220, 16, 16, "MQTT:CONNECTED   ", BLUE);
		}
		else
		{
			lcd_show_string(5, 250, 220, 16, 16, "MQTT:DISCONNECTED", RED);
		}

		vTaskDelay(pdMS_TO_TICKS(300));
	}
}

/**
 * @brief       Task_Actuator_Ctrl
 * @param       pvParameters : 传入参数(未用到)
 * @retval      无
 */
void actuator_ctrl_task(void *pvParameters)
{
	uint16_t angle = 0;

	pvParameters = pvParameters;

	/* TIM3_CH1(PA6): 50Hz, 1us计数分辨率 */
	if (btim_tim3_ch1_pwm_init(20000 - 1, 84 - 1, SG90_MID_PULSE_US) != 0)
	{
		printf("SG90 timer init failed.\r\n");
		vTaskDelete(NULL);
	}

	if (sg90_init() != 0)
	{
		printf("SG90 init failed.\r\n");
		vTaskDelete(NULL);
	}

	sg90_set_angle(0.0f);

	while (1)
	{
		if (xQueueReceive(g_actuator_queue, &angle, portMAX_DELAY) == pdTRUE)
		{
			if (angle > 180U)
			{
				angle = 180U;
			}
			sg90_set_angle((float)angle);
			printf("[SG90] set angle=%d\r\n", angle);
		}
	}
}

void iwdg_feed_task(void *pvParameters)
{
	pvParameters = pvParameters;

	while (1)
	{
		if (HAL_IWDG_Refresh(&g_iwdg_handle) != HAL_OK)
		{
			printf("IWDG refresh failed.\r\n");
		}
		else 
		{
			printf("IWDG fed.\r\n");
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

static void sensor_period_timer_cb(TimerHandle_t xTimer)
{
	(void)xTimer;
	if (g_sensor_period_sem != NULL)
	{
		xSemaphoreGive(g_sensor_period_sem);
	}
}

static uint8_t iwdg_init_3s(void)
{
	/* LSI约32KHz: 超时时间约=(Reload+1)*Prescaler/LSI */
	g_iwdg_handle.Instance = IWDG;
	g_iwdg_handle.Init.Prescaler = IWDG_PRESCALER_64;
	g_iwdg_handle.Init.Reload = 1499U; /* 约3秒 */

	if (HAL_IWDG_Init(&g_iwdg_handle) != HAL_OK)
	{
		return 1;
	}

	return 0;
}
