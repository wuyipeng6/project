/**
 ****************************************************************************************************
 * @file        sg90.c
 * @author      AI Generated
 * @version     V1.0
 * @date        2026-03-23
 * @brief       SG90 舵机驱动实现（PWM）
 ****************************************************************************************************
 */

#include "./BSP/SG90/sg90.h"
#include "./BSP/TIMER/btim.h"
#include "stm32f4xx_hal.h"

static uint16_t g_sg90_pulse_us = SG90_MID_PULSE_US;
static uint8_t g_sg90_inited = 0;

/**
 * @brief       SG90初始化
 * @retval      0, 成功
 *              1, 失败
 */
uint8_t sg90_init(void)
{
	extern TIM_HandleTypeDef g_tim3_handler;

	if (g_sg90_inited == 1)
	{
		return 0;
	}

	/* SG90驱动不负责初始化定时器，需在外部先调用btim_tim3_ch1_pwm_init() */
	if (g_tim3_handler.Instance != TIM3)
	{
		return 1;
	}

	btim_tim3_ch1_set_compare(g_sg90_pulse_us);// 设置初始脉宽

	g_sg90_inited = 1;

	return 0;
}

/**
 * @brief       SG90反初始化
 * @retval      0, 成功
 *              1, 失败
 */
uint8_t sg90_deinit(void)
{
	if (g_sg90_inited == 0)
	{
		return 0;
	}

	btim_tim3_ch1_set_compare(0);

	g_sg90_inited = 0;

	return 0;
}

/**
 * @brief       直接设置脉宽（us）
 * @param       pulse_us : 脉宽us
 * @retval      0, 成功
 *              1, 参数错误
 *              2, 未初始化
 */
uint8_t sg90_set_pulse_us(uint16_t pulse_us)
{
	if (g_sg90_inited == 0)
	{
		return 2;
	}

	if ((pulse_us < SG90_MIN_PULSE_US) || (pulse_us > SG90_MAX_PULSE_US))
	{
		return 1;
	}

	g_sg90_pulse_us = pulse_us;
	btim_tim3_ch1_set_compare(g_sg90_pulse_us);

	return 0;
}

/**
 * @brief       设置角度（0~180）
 * @param       angle : 角度
 * @retval      0, 成功
 *              2, 未初始化
 */
uint8_t sg90_set_angle(float angle)
{
	uint16_t pulse_us;

	if (g_sg90_inited == 0)
	{
		return 2;
	}

	if (angle < 0.0f)
	{
		angle = 0.0f;
	}
	else if (angle > 180.0f)
	{
		angle = 180.0f;
	}

	pulse_us = (uint16_t)(SG90_MIN_PULSE_US + (angle / 180.0f) * (float)(SG90_MAX_PULSE_US - SG90_MIN_PULSE_US));

	g_sg90_pulse_us = pulse_us;
	btim_tim3_ch1_set_compare(g_sg90_pulse_us);

	return 0;
}

/**
 * @brief       获取当前脉宽
 * @retval      当前脉宽(us)
 */
uint16_t sg90_get_pulse_us(void)
{
	return g_sg90_pulse_us;
}
