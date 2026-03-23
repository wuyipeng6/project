
#include "./BSP/TIMER/btim.h"


/*******************定时器3 CH1 输出PWM波，作为SG90舵机的控制信号输出********************/

TIM_HandleTypeDef g_tim3_handler; /* 定时器参数句柄 */

/**
 * @brief       TIM PWM底层驱动，开启时钟，初始化GPIO
 * @note        此函数会被HAL_TIM_PWM_Init()函数调用
 * @param       htim_pwm : TIM句柄
 * @retval      无
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef *htim_pwm)
{
	GPIO_InitTypeDef gpio_init_struct;

	if (htim_pwm->Instance == TIM3)
	{
		__HAL_RCC_TIM3_CLK_ENABLE();
		BTIM_TIM3_CH1_GPIO_CLK_ENABLE();

		gpio_init_struct.Pin = BTIM_TIM3_CH1_GPIO_PIN;
		gpio_init_struct.Mode = GPIO_MODE_AF_PP;
		gpio_init_struct.Pull = GPIO_NOPULL;
		gpio_init_struct.Speed = GPIO_SPEED_FREQ_HIGH;
		gpio_init_struct.Alternate = BTIM_TIM3_CH1_GPIO_AF;
		HAL_GPIO_Init(BTIM_TIM3_CH1_GPIO_PORT, &gpio_init_struct);
	}
}

/**
 * @brief       TIM3 CH1 PWM初始化（PA6）
 * @note        可用于SG90舵机脉冲输出（典型: arr=20000-1, psc=84-1 -> 50Hz, 1us分辨率）
 * @param       arr   : 自动重装值
 * @param       psc   : 预分频值
 * @param       pulse : 比较值（脉宽）
 * @retval      0,成功; 1,失败
 */
uint8_t btim_tim3_ch1_pwm_init(uint16_t arr, uint16_t psc, uint16_t pulse)
{
	TIM_OC_InitTypeDef tim_oc_handle;

	g_tim3_handler.Instance = TIM3;
	g_tim3_handler.Init.Prescaler = psc;
	g_tim3_handler.Init.CounterMode = TIM_COUNTERMODE_UP;
	g_tim3_handler.Init.Period = arr;
	g_tim3_handler.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	g_tim3_handler.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

	if (HAL_TIM_PWM_Init(&g_tim3_handler) != HAL_OK)
	{
		return 1;
	}

	tim_oc_handle.OCMode = TIM_OCMODE_PWM1;
	tim_oc_handle.Pulse = pulse;
	tim_oc_handle.OCPolarity = TIM_OCPOLARITY_HIGH;
	tim_oc_handle.OCFastMode = TIM_OCFAST_DISABLE;

	if (HAL_TIM_PWM_ConfigChannel(&g_tim3_handler, &tim_oc_handle, TIM_CHANNEL_1) != HAL_OK)
	{
		return 1;
	}

	if (HAL_TIM_PWM_Start(&g_tim3_handler, TIM_CHANNEL_1) != HAL_OK)
	{
		return 1;
	}

	return 0;
}

/**
 * @brief       设置TIM3 CH1比较值
 * @param       compare : 比较值
 * @retval      无
 */
void btim_tim3_ch1_set_compare(uint16_t compare)
{
	__HAL_TIM_SET_COMPARE(&g_tim3_handler, TIM_CHANNEL_1, compare);
}
