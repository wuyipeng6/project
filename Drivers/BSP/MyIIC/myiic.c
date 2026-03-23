/**
 ****************************************************************************************************
 * @file        myiic.c
 * @author      AI Generated
 * @version     V1.0
 * @date        2026-03-23
 * @brief       IIC1 底层初始化驱动
 ****************************************************************************************************
 */

#include "myiic.h"


/* I2C1 GPIO: PB6(SCL), PB7(SDA) */
#define MYIIC1_SCL_GPIO_PORT GPIOB
#define MYIIC1_SCL_GPIO_PIN GPIO_PIN_6
#define MYIIC1_SDA_GPIO_PORT GPIOB
#define MYIIC1_SDA_GPIO_PIN GPIO_PIN_7
#define MYIIC1_GPIO_CLK_ENABLE()      \
	do                                \
	{                                 \
		__HAL_RCC_GPIOB_CLK_ENABLE(); \
	} while (0)
#define MYIIC1_CLK_ENABLE()          \
	do                               \
	{                                \
		__HAL_RCC_I2C1_CLK_ENABLE(); \
	} while (0)

I2C_HandleTypeDef hi2c1;

static uint8_t g_myiic1_inited = 0;

/**
 * @brief       I2C MSP初始化回调（由 HAL_I2C_Init 内部调用）
 * @param       hi2c: I2C句柄
 * @retval      无
 */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
	GPIO_InitTypeDef gpio_init_struct;

	if (hi2c->Instance == I2C1)
	{
		MYIIC1_GPIO_CLK_ENABLE();
		MYIIC1_CLK_ENABLE();

		gpio_init_struct.Pin = MYIIC1_SCL_GPIO_PIN | MYIIC1_SDA_GPIO_PIN;
		gpio_init_struct.Mode = GPIO_MODE_AF_OD;
		gpio_init_struct.Pull = GPIO_PULLUP;
		gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
		gpio_init_struct.Alternate = GPIO_AF4_I2C1;
		HAL_GPIO_Init(MYIIC1_SCL_GPIO_PORT, &gpio_init_struct);
	}
}

/**
 * @brief       I2C1初始化
 * @retval      0, 成功
 *              1, 失败
 */
uint8_t myiic1_init(void)
{
	if (g_myiic1_inited == 1)
	{
		return 0;
	}

	hi2c1.Instance = I2C1;
	hi2c1.Init.ClockSpeed = 100000;						  // 标准模式下时钟频率为100kHz
	hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;				  // 标准模式下占空比为2
	hi2c1.Init.OwnAddress1 = 0;							  // 作为主机时OwnAddress1不需要设置
	hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;  // 使用7位地址模式
	hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE; // 仅使用单地址模式
	hi2c1.Init.OwnAddress2 = 0;							  // 仅在双地址模式下使用
	hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE; // 关闭通用呼叫模式
	hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;	  // 关闭时钟拉伸

	if (HAL_I2C_Init(&hi2c1) != HAL_OK)
	{
		return 1;
	}

	g_myiic1_inited = 1;

	return 0;
}
