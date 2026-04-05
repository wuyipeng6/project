#ifndef __BOOTLOADER_DEFINE_H__
#define __BOOTLOADER_DEFINE_H__




/* USING_PART_PROJECT */
#define ONE_PART_PROJECT                    0
#define DOUBLE_PART_PROJECT                 1
#define TRIPLE_PART_PROJECT                 2


#define APP_PART_NAME                       "app"
#define DOWNLOAD_PART_NAME                  "download"
#define FACTORY_PART_NAME                   "factory"

#define ONCHIP_FLASH_END_ADDRESS            ((uint32_t)(FLASH_BASE + ONCHIP_FLASH_SIZE))            /* 片内 flash 末地址 */
#define APP_ADDRESS                         ((uint32_t)(FLASH_BASE + BOOTLOADER_SIZE))              /* APP 分区起始地址 */
#define DOWNLOAD_ADDRESS                    ((uint32_t)(APP_ADDRESS + APP_PART_SIZE))               /* download 分区起始地址 */
#define FACTORY_ADDRESS                     ((uint32_t)(DOWNLOAD_ADDRESS + DOWNLOAD_PART_SIZE))     /* factory 分区起始地址 */

#endif
