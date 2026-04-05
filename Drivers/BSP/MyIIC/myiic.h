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

#include "SYSTEM/sys/sys.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/* I2C1 句柄(供上层驱动使用) */
	extern I2C_HandleTypeDef hi2c1;

	/* I2C1 初始化，成功返回0，失败返回1 */
	uint8_t myiic1_init(void);

	// 上面是IIC1的底层初始化(用于SHT20)，下面是软件IIC的操作函数（用于EEPROM）

// IO方向设置
#define SDA_IN()                         \
	{                                    \
		GPIOB->MODER &= ~(3 << (9 * 2)); \
		GPIOB->MODER |= 0 << 9 * 2;      \
	} // PB9输入模式
#define SDA_OUT()                        \
	{                                    \
		GPIOB->MODER &= ~(3 << (9 * 2)); \
		GPIOB->MODER |= 1 << 9 * 2;      \
	} // PB9输出模式
// IO操作
#define IIC_SCL PBout(8) // SCL
#define IIC_SDA PBout(9) // SDA
#define READ_SDA PBin(9) // 输入SDA

	// IIC所有操作函数
	void IIC_Init(void);				 // 初始化IIC的IO口
	void IIC_Start(void);				 // 发送IIC开始信号
	void IIC_Stop(void);				 // 发送IIC停止信号
	void IIC_Send_Byte(u8 txd);			 // IIC发送一个字节
	u8 IIC_Read_Byte(unsigned char ack); // IIC读取一个字节
	u8 IIC_Wait_Ack(void);				 // IIC等待ACK信号
	void IIC_Ack(void);					 // IIC发送ACK信号
	void IIC_NAck(void);				 // IIC不发送ACK信号

	void IIC_Write_One_Byte(u8 daddr, u8 addr, u8 data);
	u8 IIC_Read_One_Byte(u8 daddr, u8 addr);

#ifdef __cplusplus
}
#endif

#endif
