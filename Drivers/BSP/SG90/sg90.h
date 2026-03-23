/**
 ****************************************************************************************************
 * @file        sg90.h
 * @author      AI Generated
 * @version     V1.0
 * @date        2026-03-23
 * @brief       SG90 舵机驱动接口（PWM）
 ****************************************************************************************************
 */

#ifndef __SG90_H
#define __SG90_H

#include "./SYSTEM/sys/sys.h"

/*
 * 定时器输出由BTIM模块负责初始化：
 * TIM3_CH1 -> PA6 (AF2)
 * 请先调用: btim_tim3_ch1_pwm_init(20000 - 1, 84 - 1, SG90_MID_PULSE_US)
 */

/* SG90 常用参数 */
#define SG90_PWM_FREQ_HZ 50U	/* 50Hz, 周期20ms */
#define SG90_MIN_PULSE_US 500U	/* 最小脉宽(可按实物微调) */
#define SG90_MID_PULSE_US 1500U /* 中位脉宽 */
#define SG90_MAX_PULSE_US 2500U /* 最大脉宽(可按实物微调) */

uint8_t sg90_init(void);
uint8_t sg90_deinit(void);

/* 设置角度: 0~180度 */
uint8_t sg90_set_angle(float angle);

/* 直接设置脉宽: 单位us, 推荐范围500~2500 */
uint8_t sg90_set_pulse_us(uint16_t pulse_us);

/* 获取当前脉宽 */
uint16_t sg90_get_pulse_us(void);

#endif
