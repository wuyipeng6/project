#ifndef __BTIM_H
#define __BTIM_H

#include "./SYSTEM/sys/sys.h"

/******************************************************************************************/
/* 基本定时器 定义 */

/* TIM3 CH1(PA6) PWM 输出定义（用于SG90） */

#define BTIM_TIM3_CH1_GPIO_PORT GPIOA
#define BTIM_TIM3_CH1_GPIO_PIN GPIO_PIN_6
#define BTIM_TIM3_CH1_GPIO_AF GPIO_AF2_TIM3
#define BTIM_TIM3_CH1_GPIO_CLK_ENABLE() \
	do                                  \
	{                                   \
		__HAL_RCC_GPIOA_CLK_ENABLE();   \
	} while (0)

/******************************************************************************************/

uint8_t btim_tim3_ch1_pwm_init(uint16_t arr, uint16_t psc, uint16_t pulse); /* TIM3_CH1 PWM初始化 */
void btim_tim3_ch1_set_compare(uint16_t compare);							/* TIM3_CH1 设置比较值 */

#endif
