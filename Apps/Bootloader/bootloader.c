
#include "bootloader.h"

static unsigned char g_bootloader_inited = 0;

/*
 * 为了降低 bootloader_http_download_to_fal() 的栈占用，
 * 将较大的工作缓冲区放到静态区。
 * 注意：该实现为非重入，不可并发调用。
 */
static char g_http_ip[32];
static char g_http_path[192];
static char g_http_req_buf[320];
static char g_http_header_buf[1024];
static unsigned char g_http_recv_buf[1024];

static int bootloader_find_http_header_end(const char *buf, int len)
{
	int i;

	for (i = 0; i <= (len - 4); i++)
	{
		if ((buf[i] == '\r') && (buf[i + 1] == '\n') && (buf[i + 2] == '\r') && (buf[i + 3] == '\n'))
		{
			return (i + 4);
		}
	}

	return -1;
}

static int bootloader_parse_http_status_code(const char *header, int header_len)
{
	int i;
	int line_end = -1;
	int status = -1;

	for (i = 0; i < header_len - 1; i++)
	{
		if ((header[i] == '\r') && (header[i + 1] == '\n'))
		{
			line_end = i;
			break;
		}
	}

	if (line_end <= 0)
	{
		return -1;
	}

	if (sscanf(header, "HTTP/%*d.%*d %d", &status) != 1)
	{
		return -1;
	}

	return status;
}

static int bootloader_parse_http_target(const char *addr, const char *file_name,
										char *ip, size_t ip_size,
										char *path, size_t path_size,
										unsigned short *port)
{
	const char *p;
	const char *slash = NULL;
	const char *colon = NULL;
	size_t ip_len;
	size_t path_len;
	size_t i;

	if ((addr == NULL) || (ip == NULL) || (path == NULL) || (port == NULL) || (ip_size == 0) || (path_size == 0))
	{
		return -1;
	}

	p = addr;
	if (strncmp(p, "http://", 7) == 0)
	{
		p += 7;
	}

	slash = strchr(p, '/');
	if (slash == NULL)
	{
		slash = p + strlen(p);
	}

	colon = strchr(p, ':');
	if ((colon != NULL) && (colon < slash))
	{
		ip_len = (size_t)(colon - p);
		*port = (unsigned short)atoi(colon + 1);
	}
	else
	{
		ip_len = (size_t)(slash - p);
		*port = (unsigned short)BOOTLOADER_HTTP_PORT;
	}

	if ((ip_len == 0) || (ip_len >= ip_size))
	{
		return -1;
	}

	for (i = 0; i < ip_len; i++)
	{
		if (!(((p[i] >= '0') && (p[i] <= '9')) || (p[i] == '.')))
		{
			return -1;
		}
	}

	memcpy(ip, p, ip_len);
	ip[ip_len] = '\0';

	if (*slash == '\0')
	{
		path[0] = '/';
		path[1] = '\0';
	}
	else
	{
		path_len = strlen(slash);
		if (path_len >= path_size)
		{
			return -1;
		}
		memcpy(path, slash, path_len + 1);
	}

	if ((file_name != NULL) && (file_name[0] != '\0'))
	{
		size_t base_len = strlen(path);
		size_t file_len = strlen(file_name);

		if ((base_len == 1) && (path[0] == '/'))
		{
			if ((1 + file_len) >= path_size)
			{
				return -1;
			}
			memcpy(path + 1, file_name, file_len + 1);
		}
		else if (path[base_len - 1] == '/')
		{
			if ((base_len + file_len) >= path_size)
			{
				return -1;
			}
			memcpy(path + base_len, file_name, file_len + 1);
		}
	}

	if (*port == 0)
	{
		*port = (unsigned short)BOOTLOADER_HTTP_PORT;
	}

	return 0;
}

int bootloader_init(void)
{
	int fal_result = 0;

	if (g_bootloader_inited != 0)
	{
		return 0;
	}

	AT24CXX_Init();

	fal_result = fal_init();
	if (fal_result < 0)
	{
		return -1;
	}

	g_bootloader_inited = 1;

	return 0;
}

int bootloader_set_update_flag(void)
{
	u32 value = 0;

	if (bootloader_init() != 0)
	{
		return -1;
	}

	AT24CXX_WriteLenByte((u16)FLAG_ADDR_DOWNLOAD, (u32)FLAG_SET_VALUE, 4);
	value = AT24CXX_ReadLenByte((u16)FLAG_ADDR_DOWNLOAD, 4);

	return (value == (u32)FLAG_SET_VALUE) ? 0 : -1;
}

int bootloader_clear_update_flag(void)
{
	u32 value = 0;

	if (bootloader_init() != 0)
	{
		return -1;
	}

	AT24CXX_WriteLenByte((u16)FLAG_ADDR_DOWNLOAD, 0U, 4);
	value = AT24CXX_ReadLenByte((u16)FLAG_ADDR_DOWNLOAD, 4);

	return (value == 0U) ? 0 : -1;
}

int bootloader_http_download_to_fal(const char *http_url, const char *file_name, const char *fal_part_name)
{
	const struct fal_partition *part = NULL;
	struct sockaddr_in server_addr;
	int sockfd = -1;
	int ret;
	int write_ret;
	int total_written = 0;
	int header_done = 0;
	int status_code;
	unsigned int write_offset = 0;
	unsigned short port = (unsigned short)BOOTLOADER_HTTP_PORT;
	int header_len = 0;
	int header_end;
	int payload_len;
	unsigned int recv_count = 0;

	if ((http_url == NULL) || (fal_part_name == NULL) || (fal_part_name[0] == '\0'))
	{
		return -1;
	}

	if (bootloader_init() != 0)
	{
		return -2;
	}

	if (bootloader_parse_http_target(http_url, file_name, g_http_ip, sizeof(g_http_ip), g_http_path, sizeof(g_http_path), &port) != 0)
	{
		return -3;
	}

	part = fal_partition_find(fal_part_name);
	if (part == NULL)
	{
		return -4;
	}

	iwdg_feed(); // 擦除操作可能较慢，提前喂狗一次，防止复位
	if (fal_partition_erase_all(part) < 0)
	{
		return -5;
	}

	sockfd = lwip_socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		return -7;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = inet_addr(g_http_ip);
	if (server_addr.sin_addr.s_addr == INADDR_NONE)
	{
		return -6;
	}

	// 网络不好的时候，connect很慢，提前喂狗
	iwdg_feed();
	if (lwip_connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
	{
		lwip_close(sockfd);
		return -8;
	}

	snprintf(g_http_req_buf, sizeof(g_http_req_buf),
			 "GET %s HTTP/1.1\r\nHost: %s:%u\r\nConnection: close\r\n\r\n",
			 g_http_path, g_http_ip, port);

	if (lwip_send(sockfd, g_http_req_buf, strlen(g_http_req_buf), 0) < 0)
	{
		lwip_close(sockfd);
		return -9;
	}

	while ((ret = lwip_recv(sockfd, g_http_recv_buf, sizeof(g_http_recv_buf), 0)) > 0)
	{
		recv_count++;
		if ((recv_count & 0x07U) == 0U) // 每接收到8个包，喂狗一次，防止复位
		{
			iwdg_feed();
		}

		if (!header_done)
		{
			if ((header_len + ret) > (int)sizeof(g_http_header_buf))
			{
				lwip_close(sockfd);
				return -10;
			}

			memcpy(g_http_header_buf + header_len, g_http_recv_buf, (size_t)ret);
			header_len += ret;

			header_end = bootloader_find_http_header_end(g_http_header_buf, header_len);
			if (header_end < 0)
			{
				continue;
			}

			status_code = bootloader_parse_http_status_code(g_http_header_buf, header_len);
			if (status_code != 200)
			{
				lwip_close(sockfd);
				return -11;
			}

			header_done = 1;
			payload_len = header_len - header_end;
			if (payload_len > 0)
			{
				if ((write_offset + (unsigned int)payload_len) > part->len)
				{
					lwip_close(sockfd);
					return -12;
				}

				write_ret = fal_partition_write(part, write_offset, (const uint8_t *)(g_http_header_buf + header_end), (size_t)payload_len);
				if (write_ret != payload_len)
				{
					lwip_close(sockfd);
					return -13;
				}

				write_offset += (unsigned int)payload_len;
				total_written += payload_len;
			}
		}
		else
		{
			if ((write_offset + (unsigned int)ret) > part->len)
			{
				lwip_close(sockfd);
				return -14;
			}

			write_ret = fal_partition_write(part, write_offset, g_http_recv_buf, (size_t)ret);
			if (write_ret != ret)
			{
				lwip_close(sockfd);
				return -15;
			}

			write_offset += (unsigned int)ret;
			total_written += ret;
		}
	}

	lwip_close(sockfd);
	iwdg_feed();

	if ((ret < 0) || (!header_done) || (total_written <= 0))
	{
		return -16;
	}

	/* 下载成功后，置位升级标志并软件复位，重启进入 Bootloader 执行升级 */
	if (bootloader_set_update_flag() != 0)
	{
		return -17;
	}

	iwdg_feed();
	__disable_irq();
	NVIC_SystemReset();

	while (1)
	{
	}
}
