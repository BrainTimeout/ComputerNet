#include<stdio.h>
#include<stdlib.h>

#define HAVE_REMOTE
#include<pcap.h>

#pragma warning(disable:4996)
#pragma comment(lib,"packet.lib")
#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"ws2_32.lib")
#define ETHERNET_IP 0x0800
#define MAX_SIZE 2048  // 数据包的最大大小
#define CHUNK_SIZE 1460 // 每个数据包的有效载荷大小

int size_of_packet = 0;
u_int32_t crc32_table[256];

unsigned char dest_mac[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x08 } ;
unsigned char source_mac[6] = { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x08 };
// ethernet header
struct ethernet_header
{
	u_int8_t dest_mac[6];
	u_int8_t src_mac[6];
	u_int16_t ethernet_type;
};

//计算校验表
// generate crc32 table
void generate_crc32_table()
{
	int i, j;
	u_int32_t crc;
	for (i = 0; i < 256; i++)
	{
		crc = i;
		for (j = 0; j < 8; j++)
		{
			if (crc & 1)
				crc = (crc >> 1) ^ 0xEDB88320;
			else
				crc >>= 1;
		}
		crc32_table[i] = crc;
	}
}

//计算缓冲区中待发送信息的校验和
// calculate crc32 checksum
u_int32_t calculate_crc(u_int8_t *buffer, int len)
{
	int i;
	u_int32_t crc;
	crc = 0xffffffff;
	for (i = 0; i < len; i++)
	{
		crc = (crc >> 8) ^ crc32_table[(crc & 0xFF) ^ buffer[i]];
	}
	crc ^= 0xffffffff;
	return crc;
}

//载入数据报的head
// load ethernet header
void load_ethernet_header(u_int8_t *buffer)
{
	struct ethernet_header *hdr = (struct ethernet_header*)buffer;
	
	int i;
    for (i = 0; i < 6; i++)
    {
		hdr->dest_mac[i] = dest_mac[i];
        hdr->src_mac[i] = source_mac[i];
	}
    hdr->ethernet_type = ETHERNET_IP;
    size_of_packet += sizeof(struct ethernet_header);
}

// 载入数据报的载荷数据
// load ethernet data
int load_ethernet_data(u_int8_t *buffer, FILE *fp, int chunk_index)
{
	int size_of_data = 4;
	char tmp[CHUNK_SIZE];
	int ch;
	*(int*)tmp = chunk_index; //第一个数据标记序号

	//读取数据块
	//每次读入一个字节
	while (size_of_data < CHUNK_SIZE )
	{
		if((ch = fgetc(fp)) == EOF) break;
		tmp[size_of_data] = ch;
		size_of_data++;
	}

	// 如果当前数据块为空或者不合法，返回-1
	if (size_of_data == 4)
	{
		return -1;
	}
	
	// 计算CRC校验
	u_int32_t crc = calculate_crc((u_int8_t*)tmp, size_of_data);
	printf("----------------------\n");
	printf("index is : %d\n",chunk_index);
	printf("CRC is :%0x\n", crc);

	// 将数据填充到缓冲区
	int i;
	for (i = 0; i < size_of_data; i++)
	{
		*(buffer + i) = tmp[i];
	}

	// 将CRC校验值填入数据包
	*(u_int32_t*)(buffer + i) = crc;
	size_of_packet += size_of_data + 4; // 数据包大小加上CRC大小
	printf("packet size is : %d\n",size_of_packet);
	printf("data size is : %d\n",size_of_data);
	return size_of_data;
}

int main()
{	
	u_int8_t buffer[MAX_SIZE];  // 作为数据包的载体
	generate_crc32_table();
	
	int start_index,end_index;
	printf("put in start_index:");
	scanf("%d",&start_index);
	printf("put in end_index:");
	scanf("%d",&end_index);

	// 打开待发送文件
	FILE *fp = fopen("send_file", "rb");
    if (fp == NULL)
    {
        printf("Failed to open file.\n");
        return -1;
    }

	// 每次读取并发送一个数据块
	int data_size;
	int chunk_index = 0;
	while ((data_size = load_ethernet_data(buffer + sizeof(struct ethernet_header), fp, chunk_index)) > 0)
	{	
		if(chunk_index < start_index){
			size_of_packet = 0;
			chunk_index++;
			continue;
		}
		if(chunk_index>end_index) break;
		load_ethernet_header(buffer); // 加载以太网头部
		// 发送数据包
		pcap_t *handle;
		// 声明一个指向 pcap_t 结构的指针，用于管理网络接口的捕获句柄。
		char *device;
		// device保存设备信息
		char error_buffer[PCAP_ERRBUF_SIZE];
		// error_buffer保存错误信息
		device = pcap_lookupdev(error_buffer);
		// 调用 pcap_lookupdev 查找默认的网络设备（如网卡）
		if (device == NULL)
		{
			printf("%s\n", error_buffer);
			return -1;
		}
		
		// handle
		handle = pcap_open_live(device, size_of_packet, PCAP_OPENFLAG_PROMISCUOUS, 1, error_buffer);
		// 使用 pcap_open_live 打开指定网络设备的捕获句柄
		if (handle == NULL)
		{
			printf("Open adapter failed..\n");
			return -1;
		}

		if (pcap_sendpacket(handle, (const u_char*)buffer, size_of_packet) != 0)
		// 调用 pcap_sendpacket 通过捕获句柄 handle 发送数据包。
		{
			printf("Failed to send packet: %s\n", pcap_geterr(handle));
			pcap_close(handle);
			fclose(fp);
			return -1;
		}
		printf("packet(index: %d) is send successfully\n",chunk_index);
		printf("----------------------\n");
		pcap_close(handle);
		// 更新数据包大小
		size_of_packet = 0;
		chunk_index++;
	}

	// 关闭文件
	fclose(fp);
	return 0;
}