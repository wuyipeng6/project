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
#include "token.h"
#include "string.h"
#include <stdlib.h>

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

/**
 * @brief       SG90开关控制接口(在其他文件中实现)
 * @param       on_off: 1=开启舵机, 0=关闭舵机
 */
extern void sg90_onoff_ctrl(uint8_t on_off);

/**
 * @brief       从字符串中提取开关值
 * @param       payload: 负载字符串
 * @return      1=开启, 0=关闭, -1=未识别
 */
static int mqtt_extract_onoff(const char *payload)
{
	const char *p;

	if (payload == NULL || strlen(payload) == 0)
	{
		return -1;
	}

	/* 情况1: 优先查找 "SG90" 字段的值（支持 {"SG90": 1} 格式） */
	p = strstr(payload, "SG90");
	if (p != NULL)
	{
		/* 在SG90后面查找冒号 */
		while (*p && *p != ':')
			p++;
		if (*p == ':')
		{
			p++; /* 跳过冒号 */
			/* 跳过空格 */
			while (*p == ' ' || *p == '\t')
				p++;
			/* 提取数值 */
			if (*p == '1')
				return 1;
			if (*p == '0')
				return 0;
		}
	}

	/* 情况2: JSON中包含 ":1" 或 ":0" */
	p = strstr(payload, ":1");
	if (p != NULL)
	{
		char next_char = *(p + 2);
		/* 确保后面是结束符、逗号、空格或大括号，避免误匹配 :10, :12等 */
		if (next_char == '\0' || next_char == '}' || next_char == ',' ||
			next_char == ' ' || next_char == '\t' || next_char == '\r' || next_char == '\n')
		{
			return 1;
		}
	}

	p = strstr(payload, ":0");
	if (p != NULL)
	{
		char next_char = *(p + 2);
		/* 确保后面是结束符、逗号、空格或大括号，避免误匹配 :01, :02等 */
		if (next_char == '\0' || next_char == '}' || next_char == ',' ||
			next_char == ' ' || next_char == '\t' || next_char == '\r' || next_char == '\n')
		{
			return 0;
		}
	}

	/* 情况3: 纯数字（跳过前导空格、引号和大括号） */
	p = payload;
	while (*p && ((*p == ' ') || (*p == '\t') || (*p == '\r') || (*p == '\n') ||
				  (*p == '"') || (*p == '{')))
	{
		p++;
	}
	if (*p == '1')
		return 1;
	if (*p == '0')
		return 0;

	return -1;
}

/**
 * @brief       解析下行命令并分发到SG90开关控制接口
 * @param       topic: MQTT主题
 * @param       payload: 负载字符串
 */
static void mqtt_dispatch_sg90_cmd(const char *topic, const char *payload)
{
	int onoff;

	printf("[MQTT CMD] topic=%s\r\n", topic);
	printf("[MQTT CMD] payload=%s\r\n", payload);

	onoff = mqtt_extract_onoff(payload);
	if (onoff >= 0)
	{
		// sg90_onoff_ctrl((uint8_t)onoff);
		printf("[SG90] onoff=%d\r\n", onoff);
		return;
	}

	printf("[SG90] invalid command, expect 1 or 0\r\n");
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

	server = gethostbyname((char *)HOST_NAME); /* 对oneNET服务器地址解析 */
	if (server == NULL)
	{
		printf("DNS resolve failed for %s\r\n", HOST_NAME);
		lcd_show_string(5, 170, 210, 16, 16, "DNS resolve failed", RED);
		return;
	}
	memcpy(&g_mqtt_ip, server->h_addr, server->h_length); /* 把解析好的地址存放在mqtt_ip变量当中 */

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

	/* 创建MQTT客户端控制块 */
	g_mqtt_client = mqtt_client_new();
	if (g_mqtt_client == NULL)
	{
		printf("mqtt_client_new failed\r\n");
		return;
	}

	/* 连接服务器 */
	if (mqtt_client_connect(g_mqtt_client,													/* 服务器控制块 */
							&g_mqtt_ip, MQTT_PORT,											/* 服务器IP与端口号 */
							mqtt_connection_cb, LWIP_CONST_CAST(void *, &mqtt_client_info), /* 设置服务器连接回调函数 */
							&mqtt_client_info) != ERR_OK)									/* MQTT连接信息 */
	{
		printf("mqtt_client_connect failed\r\n");
		lcd_show_string(5, 210, 210, 16, 16, "mqtt connect failed", RED);
		return;
	}

	while (1)
	{
		if ((g_publish_flag == 1) && mqtt_client_is_connected(g_mqtt_client))
		{
			g_temp = 30 + rand() % 10 + 1;	  /* 温度的数据 */
			g_humid = 54.8 + rand() % 10 + 1; /* 湿度的数据 */
			sprintf((char *)g_payload_out, "{\"id\":123,\"dp\":{\"temperature\":[{\"v\":%0.1f}],\"humidity\":[{\"v\":%0.1f}]}}", g_temp, g_humid);
			g_payload_out_len = strlen((char *)g_payload_out);
			mqtt_publish(g_mqtt_client, DEVICE_PUBLISH, g_payload_out, g_payload_out_len, 1, 0, mqtt_publish_request_cb, NULL);
		}

		vTaskDelay(10000);
	}
}
