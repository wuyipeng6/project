/**
 ****************************************************************************************************
 * @file        lwip_demo
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-08-01
 * @brief       lwIP+OneNET+MQTT实验
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

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>
#include <stdio.h>
#include <netdb.h>
#include "lwip/apps/mqtt.h"
#include "lwip/dns.h"
#include "./BSP/LCD/lcd.h"
#include "lwip_comm.h"
#include "lwip_demo.h"
#include "freertos_demo.h"
#include "token.h"
#include "string.h"
#include <stdlib.h>
#include "cJSON.h"

/* oneNET参考文章：https://open.iot.10086.cn/doc/v5/develop/detail/251 */

// static const struct mqtt_connect_client_info_t mqtt_client_info =
//{
//     "MQTT",     /* 设备名 */
//     "366007",   /* 产品ID */
//     "version=2018-10-31&res=products%2F366007%2Fdevices%2FMQTT&et=1672735919&method=md5&sign=qI0pgDJnICGoPdhNi%2BHtfg%3D%3D", /* pass */
//     100,  /* keep alive */
//     NULL, /* will_topic */
//     NULL, /* will_msg */
//     0,    /* will_qos */
//     0     /* will_retain */
// #if LWIP_ALTCP && LWIP_ALTCP_TLS  /* 加密操作，我们一般不使用加密操作 */
//   , NULL
// #endif
// };

static ip_addr_t g_mqtt_ip;
static mqtt_client_t *g_mqtt_client;
float g_temp = 0;  /* 温度值 */
float g_humid = 0; /* 湿度值 */
unsigned char g_payload_out[200];
int g_payload_out_len = 0;
int g_publish_flag = 0; /* 发布成功标志位 */
onenet_info_t g_onenet_info = {"", "", "", "", ""};

/* MQTT下行数据缓存(处理分片数据流) */
static char g_incoming_topic[128] = {0};
static char g_incoming_payload[256] = {0};
static u16_t g_incoming_offset = 0;

typedef enum
{
	MQTT_SM_DNS_RESOLVE = 0,
	MQTT_SM_CONNECT,
	MQTT_SM_CONNECTING,
	MQTT_SM_ONLINE,
	MQTT_SM_BACKOFF
} mqtt_sm_state_t;

#define MQTT_RECONNECT_BACKOFF_MIN_MS 1000U
#define MQTT_RECONNECT_BACKOFF_MAX_MS 60000U

static volatile uint8_t g_mqtt_need_reconnect = 0;
static volatile uint8_t g_mqtt_online = 0;
static mqtt_sm_state_t g_mqtt_sm_state = MQTT_SM_DNS_RESOLVE;
static uint32_t g_reconnect_backoff_ms = MQTT_RECONNECT_BACKOFF_MIN_MS;
static TickType_t g_reconnect_deadline = 0;

/* 前向声明：该回调在文件后部定义，但在前面已被传给mqtt_publish() */
static void mqtt_publish_request_cb(void *arg, err_t err);

static void mqtt_release_resources(void)
{
	if (g_mqtt_client != NULL)
	{
		if (mqtt_client_is_connected(g_mqtt_client))
		{
			mqtt_disconnect(g_mqtt_client);
		}

		mqtt_client_free(g_mqtt_client);
		g_mqtt_client = NULL;
	}

	g_publish_flag = 0;
	g_mqtt_online = 0;
	app_data_set_mqtt_connected(0);
}

static void mqtt_schedule_reconnect_backoff(void)
{
	g_reconnect_deadline = xTaskGetTickCount() + pdMS_TO_TICKS(g_reconnect_backoff_ms);

	printf("[MQTT] reconnect in %lu ms\r\n", (unsigned long)g_reconnect_backoff_ms);

	if (g_reconnect_backoff_ms < MQTT_RECONNECT_BACKOFF_MAX_MS)
	{
		g_reconnect_backoff_ms <<= 1;
		if (g_reconnect_backoff_ms > MQTT_RECONNECT_BACKOFF_MAX_MS)
		{
			g_reconnect_backoff_ms = MQTT_RECONNECT_BACKOFF_MAX_MS;
		}
	}
}

/**
 * @brief       使用cJSON构建OneNET格式的数据上报JSON并发布
 * @param       client: MQTT客户端指针
 * @param       temp: 温度值
 * @param       humid: 湿度值
 * @return      0=成功, -1=失败
 * @note        自动释放cJSON对象和生成的字符串内存
 */
static int mqtt_publish_sensor_data_cJSON(mqtt_client_t *client, float temp, float humid)
{
	cJSON *root = NULL;
	cJSON *dp = NULL;
	cJSON *temp_array = NULL;
	cJSON *temp_obj = NULL;
	cJSON *humid_array = NULL;
	cJSON *humid_obj = NULL;
	char *json_str = NULL;
	int ret = -1;

	if (client == NULL)
	{
		printf("[cJSON] MQTT client is NULL\r\n");
		return -1;
	}

	/* 创建根对象 */
	root = cJSON_CreateObject();
	if (root == NULL)
	{
		printf("[cJSON] Failed to create root object\r\n");
		return -1;
	}

	/* 添加ID */
	cJSON_AddNumberToObject(root, "id", 123);

	/* 创建"dp"（数据点）对象 */
	dp = cJSON_AddObjectToObject(root, "dp");
	if (dp == NULL)
	{
		printf("[cJSON] Failed to create dp object\r\n");
		cJSON_Delete(root);
		return -1;
	}

	/* 创建温度数组 */
	temp_array = cJSON_AddArrayToObject(dp, "temperature");
	if (temp_array == NULL)
	{
		printf("[cJSON] Failed to create temperature array\r\n");
		cJSON_Delete(root);
		return -1;
	}

	/* 为温度数组添加对象 */
	temp_obj = cJSON_CreateObject();
	if (temp_obj == NULL)
	{
		printf("[cJSON] Failed to create temperature object\r\n");
		cJSON_Delete(root);
		return -1;
	}
	cJSON_AddNumberToObject(temp_obj, "v", temp);
	cJSON_AddItemToArray(temp_array, temp_obj);

	/* 创建湿度数组 */
	humid_array = cJSON_AddArrayToObject(dp, "humidity");
	if (humid_array == NULL)
	{
		printf("[cJSON] Failed to create humidity array\r\n");
		cJSON_Delete(root);
		return -1;
	}

	/* 为湿度数组添加对象 */
	humid_obj = cJSON_CreateObject();
	if (humid_obj == NULL)
	{
		printf("[cJSON] Failed to create humidity object\r\n");
		cJSON_Delete(root);
		return -1;
	}
	cJSON_AddNumberToObject(humid_obj, "v", humid);
	cJSON_AddItemToArray(humid_array, humid_obj);

	/* 生成紧凑型JSON字符串 */
	json_str = cJSON_PrintUnformatted(root);
	if (json_str == NULL)
	{
		printf("[cJSON] Failed to print JSON\r\n");
		cJSON_Delete(root);
		return -1;
	}

	/* 检查JSON字符串长度是否超过缓冲区 */
	int json_len = strlen(json_str);
	if (json_len > (int)sizeof(g_payload_out) - 1)
	{
		printf("[cJSON] JSON string too long: %d > %u\r\n", json_len, (unsigned int)sizeof(g_payload_out) - 1);
		cJSON_free(json_str);
		cJSON_Delete(root);
		return -1;
	}

	/* 复制到全局缓冲区 */
	strcpy((char *)g_payload_out, json_str);
	g_payload_out_len = json_len;

	printf("[cJSON] Generated sensor data JSON: %s\r\n", g_payload_out);

	/* 发布MQTT消息 */
	err_t err = mqtt_publish(client, DEVICE_PUBLISH, g_payload_out, g_payload_out_len, 1, 0, mqtt_publish_request_cb, NULL);
	if (err == ERR_OK)
	{
		ret = 0;
		printf("[cJSON] Publish success\r\n");
	}
	else
	{
		printf("[cJSON] Publish failed: %d\r\n", err);
		ret = -1;
	}

	/* 释放内存：首先释放JSON生成的字符串，然后释放cJSON对象 */
	cJSON_free(json_str);
	cJSON_Delete(root);

	return ret;
}

/**
 * @brief       使用cJSON解析下行命令中的角度值
 * @param       payload: 负载字符串（JSON格式）
 * @return      0~180=有效角度, -1=未识别
 * @note        支持格式: {"SG90": 90}, {"angle": 90}, {"dp": {"SG90": 90}} 等多种JSON格式
 *              自动检查类型和范围，并在完成后释放cJSON对象内存
 */
static int mqtt_extract_angle_cJSON(const char *payload)
{
	cJSON *json_root = NULL;
	cJSON *json_item = NULL;
	double angle_value = -1;

	if (payload == NULL || strlen(payload) == 0)
	{
		return -1;
	}

	/* 尝试解析JSON */
	json_root = cJSON_Parse(payload);
	if (json_root == NULL)
	{
		printf("[cJSON] Failed to parse JSON, invalid format\r\n");
		return -1;
	}

	/* 尝试方案1: 查找 "SG90" 字段 */
	json_item = cJSON_GetObjectItem(json_root, "SG90");
	if (json_item != NULL && cJSON_IsNumber(json_item))
	{
		angle_value = json_item->valuedouble;
		if (angle_value >= 0 && angle_value <= 180)
		{
			printf("[cJSON] Found SG90 field: %.0f\r\n", angle_value);
			cJSON_Delete(json_root);
			return (int)angle_value;
		}
	}

	/* 尝试方案2: 查找 "angle" 字段 */
	json_item = cJSON_GetObjectItem(json_root, "angle");
	if (json_item != NULL && cJSON_IsNumber(json_item))
	{
		angle_value = json_item->valuedouble;
		if (angle_value >= 0 && angle_value <= 180)
		{
			printf("[cJSON] Found angle field: %.0f\r\n", angle_value);
			cJSON_Delete(json_root);
			return (int)angle_value;
		}
	}

	/* 尝试方案3: 查找 "dp" 嵌套结构中的 "SG90" */
	json_item = cJSON_GetObjectItem(json_root, "dp");
	if (json_item != NULL && cJSON_IsObject(json_item))
	{
		cJSON *sg90_item = cJSON_GetObjectItem(json_item, "SG90");
		if (sg90_item != NULL && cJSON_IsNumber(sg90_item))
		{
			angle_value = sg90_item->valuedouble;
			if (angle_value >= 0 && angle_value <= 180)
			{
				printf("[cJSON] Found dp.SG90 field: %.0f\r\n", angle_value);
				cJSON_Delete(json_root);
				return (int)angle_value;
			}
		}
	}

	/* 解析失败或数值无效 */
	printf("[cJSON] No valid SG90/angle field found or value out of range\r\n");
	cJSON_Delete(json_root);
	return -1;
}

/**
 * @brief       解析下行命令并分发到SG90舵机控制接口（使用cJSON）
 * @param       topic: MQTT主题
 * @param       payload: 负载字符串（JSON格式）
 * @note        使用cJSON解析，支持多种JSON格式，具备类型检查
 */
static void mqtt_dispatch_sg90_cmd(const char *topic, const char *payload)
{
	int angle;

	printf("[MQTT CMD] topic=%s\r\n", topic);
	printf("[MQTT CMD] payload=%s\r\n", payload);

	/* 使用cJSON解析角度值 */
	angle = mqtt_extract_angle_cJSON(payload);

	if (angle >= 0)
	{
		if (app_actuator_send_angle((uint16_t)angle, pdMS_TO_TICKS(10)) == pdPASS)
		{
			printf("[SG90] enqueue angle=%d\r\n", angle);
		}
		else
		{
			printf("[SG90] enqueue failed\r\n");
		}
		return;
	}

	printf("[SG90] invalid command, expect angle 0~180\r\n");
}

/**
 * @brief       mqtt进入数据回调函数
 * @param       arg：传入的参数
 * @param       data：数据
 * @param       len：数据大小
 * @param       flags：标志
 * @retval      无
 */
static void
mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags)
{
	const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;
	u16_t copy_len;
	u16_t remain;
	LWIP_UNUSED_ARG(client_info);

	if ((data != NULL) && (len > 0))
	{
		if (g_incoming_offset < (sizeof(g_incoming_payload) - 1))
		{
			remain = (u16_t)(sizeof(g_incoming_payload) - 1 - g_incoming_offset);
			copy_len = (len < remain) ? len : remain;
			memcpy(&g_incoming_payload[g_incoming_offset], data, copy_len);
			g_incoming_offset = (u16_t)(g_incoming_offset + copy_len);
			g_incoming_payload[g_incoming_offset] = '\0';
		}
	}

	printf("\r\nMQTT incoming data: len %d, flags %d\n", (int)len, (int)flags);

	if ((flags & MQTT_DATA_FLAG_LAST) != 0)
	{
		/* 仅处理服务器下发命令主题 */
		if (strstr(g_incoming_topic, "/cmd/request/") != NULL)
		{
			mqtt_dispatch_sg90_cmd(g_incoming_topic, g_incoming_payload);
		}

		/* 清理，准备接收下一条消息 */
		g_incoming_offset = 0;
		g_incoming_payload[0] = '\0';
	}

	if (client_info == NULL)
	{
		return;
	}
	printf("\r\nMQTT client \"%s\" data cb: len %d, flags %d\n", client_info->client_id, (int)len, (int)flags);
}

/**
 * @brief       mqtt进入发布回调函数
 * @param       arg：传入的参数
 * @param       topic：主题
 * @param       tot_len：主题大小
 * @retval      无
 */
static void
mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len)
{
	const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

	/* 每条新消息到来时，记录topic并清空负载缓存 */
	strncpy(g_incoming_topic, topic, sizeof(g_incoming_topic) - 1);
	g_incoming_topic[sizeof(g_incoming_topic) - 1] = '\0';
	g_incoming_offset = 0;
	g_incoming_payload[0] = '\0';

	if (client_info == NULL)
	{
		printf("\r\nMQTT publish cb: topic %s, len %d\r\n", topic, (int)tot_len);
		return;
	}

	printf("\r\nMQTT client \"%s\" publish cb: topic %s, len %d\r\n", client_info->client_id,
		   topic, (int)tot_len);
}

/**
 * @brief       mqtt发布回调函数
 * @param       arg：传入的参数
 * @param       err：错误值
 * @retval      无
 */
static void
mqtt_publish_request_cb(void *arg, err_t err)
{
	LWIP_UNUSED_ARG(arg);
	if (err == ERR_OK)
	{
		printf("publish success\r\n");
	}
	else
	{
		printf("publish failed, err=%d\r\n", (int)err);
	}
}

/**
 * @brief       mqtt订阅响应回调函数
 * @param       arg：传入的参数
 * @param       err：错误值
 * @retval      无
 */
static void
mqtt_request_cb(void *arg, err_t err)
{
	const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

	if (err == ERR_OK)
	{
		g_publish_flag = 1;
	}

	if (client_info == NULL)
	{
		printf("\r\nMQTT request cb: err %d\r\n", (int)err);
		return;
	}
	printf("\r\nMQTT client \"%s\" request cb: err %d\r\n", client_info->client_id, (int)err);
}

/**
 * @brief       mqtt连接回调函数
 * @param       client：客户端控制块
 * @param       arg：传入的参数
 * @param       status：连接状态
 * @retval      无
 */
static void
mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status)
{
	err_t err;

	const struct mqtt_connect_client_info_t *client_info = (const struct mqtt_connect_client_info_t *)arg;

	LWIP_UNUSED_ARG(client);

	printf("\r\nMQTT client \"%s\" connection cb: status %d\r\n", client_info->client_id, (int)status);

	/* 判断是否连接 */
	if (status == MQTT_CONNECT_ACCEPTED)
	{
		app_data_set_mqtt_connected(1);
		g_mqtt_online = 1;
		g_mqtt_need_reconnect = 0;
		g_reconnect_backoff_ms = MQTT_RECONNECT_BACKOFF_MIN_MS;
		g_publish_flag = 0;

		/* 判断是否连接 */
		if (mqtt_client_is_connected(client))
		{
			/* 设置传入发布请求的回调 */
			mqtt_set_inpub_callback(g_mqtt_client,
									mqtt_incoming_publish_cb,
									mqtt_incoming_data_cb,
									arg);

			/* 订阅操作，并设置订阅响应会回调函数mqtt_sub_request_cb */
			err = mqtt_subscribe(client, DEVICE_SUBSCRIBE, 1, mqtt_request_cb, arg);

			if (err == ERR_OK)
			{
				printf("mqtt_subscribe return: %d\n", err);
				lcd_show_string(5, 170, 210, 16, 16, "mqtt_subscribe succeed", BLUE);
			}

			/* 订阅服务器下发的命令 */
			err = mqtt_subscribe(client, SERVER_PUBLISH, 1, mqtt_request_cb, arg);

			/* 判断是否订阅成功 */
			if (err == ERR_OK)
			{
				lcd_show_string(5, 190, 210, 16, 16, "mqtt_subscribe cmd succeed", BLUE);
			}
		}
	}
	else /* 连接失败 */
	{
		app_data_set_mqtt_connected(0);
		g_mqtt_online = 0;
		g_publish_flag = 0;
		g_mqtt_need_reconnect = 1;
		printf("mqtt_connection_cb: Disconnected, reason: %d\n", status);
	}
}

/**
 * @brief       lwip_demo进程
 * @param       无
 * @retval      无
 */
void lwip_demo(void)
{
	char pro_id[] = USER_PRODUCT_ID;	 /* 产品ID */
	char access_key[] = USER_ACCESS_KEY; /* 产品密钥 */
	char dev_name[] = USER_DEVICE_NAME;	 /* 设备名称 */
	char dev_id[] = USER_DEVICE_ID;		 /* 产品设备ID */
	char key[] = USER_KEY;				 /* 设备密钥 */
	struct hostent *server;
	static struct mqtt_connect_client_info_t mqtt_client_info;

	char version[] = "2018-10-31";
	unsigned int expiration_time = 1956499200;
	char authorization_buf[256] = {0};
	err_t mqtt_err;

	/* 在静态IP模式下，优先使用本地网关(Windows ICS)作为DNS */
	if (ip_addr_isany(dns_getserver(0)))
	{
		ip_addr_t dns0;
		IP4_ADDR(&dns0,
				 g_lwipdev.gateway[0],
				 g_lwipdev.gateway[1],
				 g_lwipdev.gateway[2],
				 g_lwipdev.gateway[3]);
		dns_setserver(0, &dns0);
		printf("DNS server set to %d.%d.%d.%d\r\n",
			   g_lwipdev.gateway[0],
			   g_lwipdev.gateway[1],
			   g_lwipdev.gateway[2],
			   g_lwipdev.gateway[3]);
	}

	/* 把各个参数保存在g_onenet_info结构体的成员变量中 */
	memset(g_onenet_info.pro_id, 0, sizeof(g_onenet_info.pro_id));
	strcpy(g_onenet_info.pro_id, pro_id);

	memset(g_onenet_info.access_key, 0, sizeof(g_onenet_info.access_key));
	strcpy(g_onenet_info.access_key, access_key);

	memset(g_onenet_info.dev_name, 0, sizeof(g_onenet_info.dev_name));
	strcpy(g_onenet_info.dev_name, dev_name);

	memset(g_onenet_info.dev_id, 0, sizeof(g_onenet_info.dev_id));
	strcpy(g_onenet_info.dev_id, dev_id);

	memset(g_onenet_info.key, 0, sizeof(g_onenet_info.key));
	strcpy(g_onenet_info.key, key);

	/* 认证密码来源：优先可切换为静态PASSWORD，便于快速定位鉴权问题 */
#if USE_ONENET_STATIC_PASSWORD
	strncpy(authorization_buf, PASSWORD, sizeof(authorization_buf) - 1);
	authorization_buf[sizeof(authorization_buf) - 1] = '\0';
#else
	onenet_authorization(version,
						 (char *)g_onenet_info.pro_id,
						 expiration_time,
						 (char *)g_onenet_info.key,
						 (char *)g_onenet_info.dev_name,
						 authorization_buf,
						 sizeof(authorization_buf),
						 0);
#endif

	if (authorization_buf[0] == '\0')
	{
		printf("OneNET token generate failed\r\n");
		lcd_show_string(5, 190, 210, 16, 16, "token generate failed", RED);
		return;
	}

	/* 设置一个空的客户端信息结构 */
	memset(&mqtt_client_info, 0, sizeof(mqtt_client_info));

	/* 设置客户端的信息量 */
	mqtt_client_info.client_id = (char *)g_onenet_info.dev_name; /* 设备名称 */
	mqtt_client_info.client_user = (char *)g_onenet_info.pro_id; /* 产品ID */
	mqtt_client_info.client_pass = (char *)authorization_buf;	 /* 计算出来的密码 */
	mqtt_client_info.keep_alive = 100;							 /* 保活时间 */
	mqtt_client_info.will_msg = NULL;							 // 遗嘱消息，
	mqtt_client_info.will_qos = 0;								 // 遗嘱消息的服务质量，
	mqtt_client_info.will_retain = 0;							 // 遗嘱消息的保留，
	mqtt_client_info.will_topic = 0;							 // 遗嘱消息的主题，

	g_mqtt_sm_state = MQTT_SM_DNS_RESOLVE;
	g_mqtt_need_reconnect = 0;
	g_mqtt_online = 0;
	g_reconnect_backoff_ms = MQTT_RECONNECT_BACKOFF_MIN_MS;

	while (1)
	{
		switch (g_mqtt_sm_state)
		{
		case MQTT_SM_DNS_RESOLVE:
			server = gethostbyname((char *)HOST_NAME);
			if (server == NULL)
			{
				printf("DNS resolve failed for %s\r\n", HOST_NAME);
				lcd_show_string(5, 170, 210, 16, 16, "DNS resolve failed", RED);
				mqtt_release_resources();
				mqtt_schedule_reconnect_backoff();
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
				break;
			}

			memcpy(&g_mqtt_ip, server->h_addr, server->h_length);
			g_mqtt_sm_state = MQTT_SM_CONNECT;
			break;

		case MQTT_SM_CONNECT:
			mqtt_release_resources();

			g_mqtt_client = mqtt_client_new();
			if (g_mqtt_client == NULL)
			{
				printf("mqtt_client_new failed\r\n");
				mqtt_schedule_reconnect_backoff();
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
				break;
			}

			mqtt_err = mqtt_client_connect(g_mqtt_client,
										   &g_mqtt_ip, MQTT_PORT,
										   mqtt_connection_cb, LWIP_CONST_CAST(void *, &mqtt_client_info),
										   &mqtt_client_info);

			if (mqtt_err != ERR_OK)
			{
				printf("mqtt_client_connect failed, err=%d\r\n", (int)mqtt_err);
				lcd_show_string(5, 210, 210, 16, 16, "mqtt connect failed", RED);
				mqtt_release_resources();
				mqtt_schedule_reconnect_backoff();
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
				break;
			}

			g_mqtt_sm_state = MQTT_SM_CONNECTING;
			break;

		case MQTT_SM_CONNECTING:
			if (g_lwipdev.link_status != LWIP_LINK_ON)
			{
				mqtt_release_resources();
				mqtt_schedule_reconnect_backoff();
				g_mqtt_need_reconnect = 0;
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
			}
			else if (g_mqtt_online)
			{
				g_mqtt_sm_state = MQTT_SM_ONLINE;
			}
			else if (g_mqtt_need_reconnect)
			{
				mqtt_release_resources();
				mqtt_schedule_reconnect_backoff();
				g_mqtt_need_reconnect = 0;
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
			}
			break;

		case MQTT_SM_ONLINE:
			if ((g_mqtt_need_reconnect != 0) ||
				(g_lwipdev.link_status != LWIP_LINK_ON) ||
				(g_mqtt_client == NULL) ||
				(!mqtt_client_is_connected(g_mqtt_client)))
			{
				printf("[MQTT] TCP disconnected, prepare reconnect\r\n");
				mqtt_release_resources();
				mqtt_schedule_reconnect_backoff();
				g_mqtt_need_reconnect = 0;
				g_mqtt_sm_state = MQTT_SM_BACKOFF;
				break;
			}

			if (g_publish_flag == 1)
			{
				app_runtime_data_t snapshot;
				app_data_get_snapshot(&snapshot);
				g_temp = snapshot.temperature;
				g_humid = snapshot.humidity;

				/* 使用cJSON构建JSON数据并发布 */
				if (mqtt_publish_sensor_data_cJSON(g_mqtt_client, g_temp, g_humid) == 0)
				{
					printf("[MQTT] Sensor data published successfully\r\n");
				}
				else
				{
					printf("[MQTT] Sensor data publish failed\r\n");
				}
			}
			break;

		case MQTT_SM_BACKOFF:
			if ((int32_t)(xTaskGetTickCount() - g_reconnect_deadline) >= 0)
			{
				g_mqtt_sm_state = MQTT_SM_DNS_RESOLVE;
			}
			break;

		default:
			g_mqtt_sm_state = MQTT_SM_DNS_RESOLVE;
			break;
		}

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}
