/*
 * This file is part of the Serial Flash Universal Driver Library.
 *
 * Copyright (c) 2016-2018, Armink, <armink.ztl@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * 'Software'), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Function: Portable interface for each platform.
 * Created on: 2016-04-23
 */

#include <sfud.h>
#include <stdarg.h>
#include "FreeRTOS.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"

static char log_buf[256];

// 用户根据实际硬件修改以下外部变量
SPI_HandleTypeDef hspi1; // SPI句柄
#define SFUD_SPI_HANDLE hspi1
#define SFUD_CS_GPIO_Port GPIOB // 修改为你的CS端口
#define SFUD_CS_Pin GPIO_PIN_14 // 修改为你的CS引脚

static SemaphoreHandle_t spi_mutex = NULL;

void sfud_log_debug(const char *file, const long line, const char *format, ...);
void sfud_log_info(const char *format, ...);
/**
 * SPI write data then read data
 */
static sfud_err spi_write_read(const sfud_spi *spi, const uint8_t *write_buf, size_t write_size, uint8_t *read_buf, size_t read_size)
{
    sfud_err result = SFUD_SUCCESS;

    // 1. CS 拉低
    HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_RESET);

    // 2. 发送指令和地址 (Write phase)
    if (write_size > 0 && write_buf != NULL)
    {
        if (HAL_SPI_Transmit(&SFUD_SPI_HANDLE, (uint8_t *)write_buf, write_size, 1000) != HAL_OK)
        {
            result = SFUD_ERR_WRITE;
        }
    }

    // 3. 一次性读取所有数据 (Read phase)
    if (result == SFUD_SUCCESS && read_size > 0 && read_buf != NULL)
    {
        // HAL_SPI_Receive 会自动在 MOSI 上发送 0xFF 或 0x00 来产生时钟
        // 这种方式比你写的单字节循环快得多，且时钟连续性更好
        if (HAL_SPI_Receive(&SFUD_SPI_HANDLE, read_buf, read_size, 1000) != HAL_OK)
        {
            result = SFUD_ERR_READ;
        }
    }

    // 4. CS 拉高 (务必确保执行，哪怕出错)
    HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_SET);

    return result;
}

#ifdef SFUD_USING_QSPI
/**
 * read flash data by QSPI
 */
static sfud_err qspi_read(const struct __sfud_spi *spi, uint32_t addr, sfud_qspi_read_cmd_format *qspi_read_cmd_format,
						  uint8_t *read_buf, size_t read_size)
{
	sfud_err result = SFUD_SUCCESS;

	/**
	 * add your qspi read flash data code
	 */

	return result;
}
#endif /* SFUD_USING_QSPI */

static void spi_lock(const sfud_spi *spi)
{
	if (spi_mutex)
		xSemaphoreTake(spi_mutex, portMAX_DELAY);
}

static void spi_unlock(const sfud_spi *spi)
{
	if (spi_mutex)
		xSemaphoreGive(spi_mutex);
}

// 这个 100μs 延时函数主要用于 轮询（Polling）等待 Flash 忙状态（Busy Bit）
// 在某些操作（如擦除或写入）完成之前，Flash 可能会处于忙状态，此时需要等待一段时间再进行下一步操作
// 使用阻塞延时就可以，100us不能多，多了之后每一次写操作（如写入一个页面）都会增加额外的延时，影响性能；
static void retry_delay_100us(void)
{
	delay_us(100);
}


sfud_err sfud_spi_port_init(sfud_flash *flash)
{
	// SPI1 RCC、GPIO、SPI初始化
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	// 1. 使能SPI1和GPIOB时钟
	__HAL_RCC_SPI1_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	// 1.1 配置CS引脚PB14为普通输出推挽，并默认拉高
	GPIO_InitStruct.Pin = SFUD_CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(SFUD_CS_GPIO_Port, &GPIO_InitStruct);
	HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_SET);

	// 2. 配置 PB4(MISO), PB5(MOSI)为复用推挽 默认上拉
	GPIO_InitStruct.Pin =  GPIO_PIN_4 | GPIO_PIN_5;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;  // 根据实际情况选择上拉或下拉，通常SPI引脚需要上拉或下拉以确保空闲状态稳定
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// 2.1 配置 PB3(SCK)为复用推挽，mode1情况默认下拉
	GPIO_InitStruct.Pin =  GPIO_PIN_3;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	// 3. SPI1参数配置
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8; // 初始化阶段先降速，提升识别稳定性
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 7;
	HAL_SPI_DeInit(&hspi1);
	HAL_SPI_Init(&hspi1);


	uint8_t cmd = 0x66;//使能reset命令
	HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_RESET);
	(void)HAL_SPI_Transmit(&SFUD_SPI_HANDLE, &cmd, 1, 1000);
	HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_SET);

	cmd = 0x99;//reset命令，必须发送，否则flash对第一个指令无响应
	HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_RESET);
	(void)HAL_SPI_Transmit(&SFUD_SPI_HANDLE, &cmd, 1, 1000);
	HAL_GPIO_WritePin(SFUD_CS_GPIO_Port, SFUD_CS_Pin, GPIO_PIN_SET);

	// 初始化互斥锁
	if (spi_mutex == NULL)
	{
		spi_mutex = xSemaphoreCreateMutex();
	}
	flash->spi.wr = spi_write_read;
	flash->spi.lock = spi_lock;
	flash->spi.unlock = spi_unlock;
	flash->spi.user_data = NULL;
	flash->retry.delay = retry_delay_100us;
	flash->retry.times = 10000;
	return SFUD_SUCCESS;
}

/**
 * This function is print debug info.
 *
 * @param file the file which has call this function
 * @param line the line number which has call this function
 * @param format output format
 * @param ... args
 */
void sfud_log_debug(const char *file, const long line, const char *format, ...)
{
	va_list args;

	/* args point to the first variable parameter */
	va_start(args, format);
	printf("[SFUD](%s:%ld) ", file, line);
	/* must use vprintf to print */
	vsnprintf(log_buf, sizeof(log_buf), format, args);
	printf("%s\n", log_buf);
	va_end(args);
}

/**
 * This function is print routine info.
 *
 * @param format output format
 * @param ... args
 */
void sfud_log_info(const char *format, ...)
{
	va_list args;

	/* args point to the first variable parameter */
	va_start(args, format);
	printf("[SFUD]");
	/* must use vprintf to print */
	vsnprintf(log_buf, sizeof(log_buf), format, args);
	printf("%s\n", log_buf);
	va_end(args);
}
