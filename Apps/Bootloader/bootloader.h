#ifndef __MY_BOOTLOADER_H__
#define __MY_BOOTLOADER_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "bootloader_define.h"
#include "bootloader_config.h"

#include "fal.h"
#include "BSP/24CXX/24cxx.h"
#include "lwip/sockets.h"
#include "BSP/IWDG/iwdg.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* OTA 下载默认参数（可按项目修改） */
#define BOOTLOADER_HTTP_IP "192.168.137.1"
#define BOOTLOADER_HTTP_PORT 9000
#define BOOTLOADER_FILE_NAME "app.fpk"
#define BOOTLOADER_FAL_NAME DOWNLOAD_PART_NAME

	int bootloader_init(void);
	int bootloader_set_update_flag(void);
	int bootloader_clear_update_flag(void);
	int bootloader_http_download_to_fal(const char *http_url, const char *file_name, const char *fal_part_name);

#ifdef __cplusplus
}
#endif

#endif
