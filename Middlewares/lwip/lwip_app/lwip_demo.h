/**
 ****************************************************************************************************
 * @file        lwip_demo.h
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2020-04-04
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

#ifndef _LWIP_DEMO_H
#define _LWIP_DEMO_H
#include "./SYSTEM/sys/sys.h"

/* 用户需要根据设备信息完善以下宏定义中的三元组内容 */
#define USER_DEVICE_NAME "MY_MQTT"							           /* 设备名 */
#define USER_PRODUCT_ID "8q4Ad1z2jn"								   /* 产品ID */
#define USER_ACCESS_KEY "s6W5Qf29VVc1gaUfA5VBt+XZbyPBC/Wpk7Ntf7vNE0E=" /* 产品密钥 */
#define USER_DEVICE_ID "2561024859"									   /* 产品设备ID */
#define USER_KEY "RTdyTTMwcm5Cd1YzSTFOZGQ2TTVFQUpVMWMxV0lCc0Q="		   /* 设备密钥 */
/* 该密码需要onenet提供的token软件计算得出 */
#define PASSWORD "version=2018-10-31&res=products%2F8q4Ad1z2jn%2Fdevices%2FMY_MQTT&et=1798732799&method=md5&sign=AtwcRCuevTvN1Wr9HXTXKA%3D%3D"
/* 连接调试开关: 1=直接使用上面的PASSWORD; 0=运行时根据USER_KEY生成token */
#define USE_ONENET_STATIC_PASSWORD 0

/* 以下参数的宏定义固定，不需要修改，只修改上方的参数即可 */
#define HOST_NAME "mqtts.heclouds.com"													/*新版onenet域名 */
#define DEVICE_SUBSCRIBE "$sys/" USER_PRODUCT_ID "/" USER_DEVICE_NAME "/dp/post/json/+" /* 订阅 */
#define DEVICE_PUBLISH "$sys/" USER_PRODUCT_ID "/" USER_DEVICE_NAME "/dp/post/json"		/* 发布 */
#define SERVER_PUBLISH "$sys/" USER_PRODUCT_ID "/" USER_DEVICE_NAME "/cmd/request/+"	/* 服务器下发命令 */

typedef struct
{
	char pro_id[16];
	char access_key[48];
	char dev_name[64 + 1];
	char dev_id[16];
	char key[48];
} onenet_info_t;

void lwip_demo(void);

#endif /* _CLIENT_H */
