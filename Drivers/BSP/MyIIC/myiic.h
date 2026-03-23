/**
 ****************************************************************************************************
 * @file        myiic.h
 * @author      AI Generated
 * @version     V1.0
 * @date        2026-03-23
 * @brief       IIC1 底层初始化驱动
 ****************************************************************************************************
 */

#ifndef __MYIIC_H
#define __MYIIC_H

#include "./SYSTEM/sys/sys.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/* I2C1 句柄(供上层驱动使用) */
	extern I2C_HandleTypeDef hi2c1;

	/* I2C1 初始化，成功返回0，失败返回1 */
	uint8_t myiic1_init(void);

#ifdef __cplusplus
}
#endif

#endif
