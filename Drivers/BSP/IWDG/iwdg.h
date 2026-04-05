#ifndef __IWDG_BSP_H
#define __IWDG_BSP_H

#include "./SYSTEM/sys/sys.h"

#ifdef __cplusplus
extern "C"
{
#endif

	/*
	 * IWDG 基础封装：
	 * - iwdg_init_3s(): 初始化约 3 秒超时的独立看门狗
	 * - iwdg_feed(): 喂狗
	 */
	uint8_t iwdg_init_3s(void);
	void iwdg_feed(void);

#ifdef __cplusplus
}
#endif

#endif /* __IWDG_BSP_H */
