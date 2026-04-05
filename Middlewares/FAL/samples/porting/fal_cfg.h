/*
 * File      : fal_cfg.h
 * This file is part of FAL (Flash Abstraction Layer) package
 * COPYRIGHT (C) 2006 - 2018, RT-Thread Development Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Change Logs:
 * Date           Author       Notes
 * 2018-05-17     armink       the first version
 */

#ifndef _FAL_CFG_H_
#define _FAL_CFG_H_

#include "bootloader_config.h"

/* 默认启用 SFUD */
#define FAL_USING_SFUD_PORT

/* 使用定义好的表，不让程序去 flash 自动搜索 */
#define FAL_PART_HAS_TABLE_CFG

#define FAL_ONCHIP_FLASH_DEV_NAME "onchip_flash"
#define FAL_SPI_FLASH_DEV_NAME "spi_flash_NM25Q128EVB"

/* 新增的 flash 在此添加 */
/* ===================== Flash device Configuration ========================= */

extern const struct fal_flash_dev stm32_onchip_flash;
extern struct fal_flash_dev spi_flash1;


#define FAL_FLASH_DEV_TABLE  \
	{                        \
		&stm32_onchip_flash, \
		&spi_flash1,         \
	}


/* ===================== Partition Configuration =========================== */
/* 情形三： download 分区和 factory 分区都在 SPI flash */
#define FAL_PART_TABLE                               \
	{                                                \
		{                                            \
			.magic_word = FAL_PART_MAGIC_WORD,       \
			.name = "bootload",                      \
			.flash_name = FAL_ONCHIP_FLASH_DEV_NAME, \
			.offset = 0,                             \
			.len = BOOTLOADER_SIZE,                  \
			.reserved = 0,                           \
		},                                           \
		{                                            \
			.magic_word = FAL_PART_MAGIC_WORD,       \
			.name = APP_PART_NAME,                   \
			.flash_name = FAL_ONCHIP_FLASH_DEV_NAME, \
			.offset = BOOTLOADER_SIZE,               \
			.len = APP_PART_SIZE,                    \
			.reserved = 0,                           \
		},                                           \
		{                                            \
			.magic_word = FAL_PART_MAGIC_WORD,       \
			.name = DOWNLOAD_PART_NAME,              \
			.flash_name = FAL_SPI_FLASH_DEV_NAME,    \
			.offset = 0,                             \
			.len = DOWNLOAD_PART_SIZE,               \
			.reserved = 0,                           \
		},                                           \
		{                                            \
			.magic_word = FAL_PART_MAGIC_WORD,       \
			.name = FACTORY_PART_NAME,               \
			.flash_name = FAL_SPI_FLASH_DEV_NAME,    \
			.offset = DOWNLOAD_PART_SIZE,            \
			.len = FACTORY_PART_SIZE,                \
			.reserved = 0,                           \
		},                                           \
	}


#endif

