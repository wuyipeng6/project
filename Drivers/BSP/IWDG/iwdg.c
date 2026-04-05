#include "iwdg.h"
#include "stm32f4xx_hal_iwdg.h"

static IWDG_HandleTypeDef g_iwdg_handle;
static uint8_t g_iwdg_inited = 0;

/**
 * @brief       初始化独立看门狗，超时时间约 3 秒
 * @param       无
 * @retval      0: 成功, 1: 失败
 */
uint8_t iwdg_init_3s(void)
{
	if (g_iwdg_inited != 0)
	{
		return 0;
	}

	g_iwdg_handle.Instance = IWDG;
	g_iwdg_handle.Init.Prescaler = IWDG_PRESCALER_64;
	g_iwdg_handle.Init.Reload = 1499U; /* 约 3 秒 */

	if (HAL_IWDG_Init(&g_iwdg_handle) != HAL_OK)
	{
		return 1;
	}

	g_iwdg_inited = 1;
	return 0;
}

/**
 * @brief       喂独立看门狗
 * @param       无
 * @retval      无
 */
void iwdg_feed(void)
{
	if (g_iwdg_inited == 0)
	{
		return;
	}

	(void)HAL_IWDG_Refresh(&g_iwdg_handle);
}
