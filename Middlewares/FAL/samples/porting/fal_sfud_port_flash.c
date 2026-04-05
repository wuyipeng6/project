/*
 * FAL <-> SFUD port for SPI NOR flash (W25Q128)
 */

#include <fal.h>
#include <sfud.h>

/* W25Q128: 16MB total size, 4KB sector */
#define W25Q128_TOTAL_SIZE (16 * 1024 * 1024UL)
#define W25Q128_SECTOR_SIZE (4 * 1024UL)

static const sfud_flash *g_sfud_dev = NULL;

static int init(void)
{
	if (g_sfud_dev != NULL)
	{
		return 0;
	}

	if (sfud_init() != SFUD_SUCCESS)
	{
		return -1;
	}

	g_sfud_dev = sfud_get_device_table();
	if (g_sfud_dev == NULL)
	{
		return -1;
	}

	return 0;
}

static int read(long offset, uint8_t *buf, size_t size)
{
	if (buf == NULL || size == 0)
	{
		return 0;
	}

	if (g_sfud_dev == NULL && init() != 0)
	{
		return -1;
	}

	if (sfud_read(g_sfud_dev, (uint32_t)offset, size, buf) != SFUD_SUCCESS)
	{
		return -1;
	}

	return (int)size;
}

static int write(long offset, const uint8_t *buf, size_t size)
{
	if (buf == NULL || size == 0)
	{
		return 0;
	}

	if (g_sfud_dev == NULL && init() != 0)
	{
		return -1;
	}

	if (sfud_write(g_sfud_dev, (uint32_t)offset, size, buf) != SFUD_SUCCESS)
	{
		return -1;
	}

	return (int)size;
}

static int erase(long offset, size_t size)
{
	if (size == 0)
	{
		return 0;
	}

	if (g_sfud_dev == NULL && init() != 0)
	{
		return -1;
	}

	if (sfud_erase(g_sfud_dev, (uint32_t)offset, size) != SFUD_SUCCESS)
	{
		return -1;
	}

	return (int)size;
}

struct fal_flash_dev spi_flash1 =
	{
		.name = FAL_SPI_FLASH_DEV_NAME,
		.addr = 0,
		.len = W25Q128_TOTAL_SIZE,
		.blk_size = W25Q128_SECTOR_SIZE,

		.ops.init = init,
		.ops.read = read,
		.ops.write = write,
		.ops.erase = erase,

		.write_gran = 1,
};
