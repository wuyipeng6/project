
#include "bootloader_define.h"

/**
 * 【选择分区方案】
 * 解释:
 *    APP:      可运行的固件区域
 *    download: 用于更新固件时的固件临时存放区域
 *    factory:  用于存放可在紧急情况下恢复的固件使用区域
 *    ONE_PART_PROJECT:     单分区方案（ APP ）
 *    DOUBLE_PART_PROJECT:  双分区方案（ APP + download ）
 *    TRIPLE_PART_PROJECT:  三分区方案（ APP + download + factory ）
 * 选项:
 *    ONE_PART_PROJECT      或 0
 *    DOUBLE_PART_PROJECT   或 1
 *    TRIPLE_PART_PROJECT   或 2
 */
#define USING_PART_PROJECT TRIPLE_PART_PROJECT

/**
 * 【 flash 相关配置项】
 * 说明:
 *    配置各个分区的大小
 * 注意事项:
 *    ！！！片内 Flash 需进行页对齐， 分区首地址必须是 Flash 的 每个独立 page 或 sector 的首地址，否则固件无法运行！！！
 *    ！！！放置在 SPI Flash 的分区至少需要最小擦写粒度的整数倍为单位进行对齐，建议以 sector 的整数倍为单位进行对齐！！！
 */
#define ONCHIP_FLASH_SIZE (1024 * 1024)	   /* 片上 flash 容量，单位: byte */
#define BOOTLOADER_SIZE (64 * 1024)		   /* 预留给 bootloader 的空间，单位: byte（最小需要大于本工程编译后的大小） */
#define APP_PART_SIZE (960 * 1024)		   /* 预留给 APP 分区的空间，单位: byte（注意页对齐） */
#define DOWNLOAD_PART_SIZE (APP_PART_SIZE) /* 预留给 download 分区的空间，单位: byte（注意页对齐，不使用时，写0） */
#define FACTORY_PART_SIZE (APP_PART_SIZE)  /* 预留给 factory 分区的空间，单位: byte（注意页对齐，不使用时，写0） */



/**
 * 【Bootloader 标志位地址配置（EEPROM）】
 * 说明:
 *    1. 地址是 24C02 EEPROM 内部地址（单位: byte）
 *    2. 标志占用 4 字节
 *    3. APP 侧写入 FLAG_SET_VALUE 代表置位；Bootloader 更新完成后清零
 */
#define FLAG_ADDR_DOWNLOAD 0x0000U
#define FLAG_SET_VALUE 0xA5A5A5A5U









