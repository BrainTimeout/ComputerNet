#include<stdio.h>
#include<stdlib.h>

#define HAVE_REMOTE
#include<pcap.h>

#pragma warning(disable:4996)
#pragma comment(lib,"packet.lib")
#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"ws2_32.lib")
#define ETHERNET_IP 0x0800
#define MAX_SIZE 2048  // ���ݰ�������С
#define CHUNK_SIZE 1460 // ÿ�����ݰ�����Ч�غɴ�С

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

//����У���
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

//���㻺�����д�������Ϣ��У���
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

//�������ݱ���head
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

// �������ݱ����غ�����
// load ethernet data
int load_ethernet_data(u_int8_t *buffer, FILE *fp, int chunk_index)
{
	int size_of_data = 4;
	char tmp[CHUNK_SIZE];
	int ch;
	*(int*)tmp = chunk_index; //��һ�����ݱ�����

	//��ȡ���ݿ�
	//ÿ�ζ���һ���ֽ�
	while (size_of_data < CHUNK_SIZE )
	{
		if((ch = fgetc(fp)) == EOF) break;
		tmp[size_of_data] = ch;
		size_of_data++;
	}

	// �����ǰ���ݿ�Ϊ�ջ��߲��Ϸ�������-1
	if (size_of_data == 4)
	{
		return -1;
	}
	
	// ����CRCУ��
	u_int32_t crc = calculate_crc((u_int8_t*)tmp, size_of_data);
	printf("----------------------\n");
	printf("index is : %d\n",chunk_index);
	printf("CRC is :%0x\n", crc);

	// ��������䵽������
	int i;
	for (i = 0; i < size_of_data; i++)
	{
		*(buffer + i) = tmp[i];
	}

	// ��CRCУ��ֵ�������ݰ�
	*(u_int32_t*)(buffer + i) = crc;
	size_of_packet += size_of_data + 4; // ���ݰ���С����CRC��С
	printf("packet size is : %d\n",size_of_packet);
	printf("data size is : %d\n",size_of_data);
	return size_of_data;
}

int main()
{	
	u_int8_t buffer[MAX_SIZE];  // ��Ϊ���ݰ�������
	generate_crc32_table();
	
	int start_index,end_index;
	printf("put in start_index:");
	scanf("%d",&start_index);
	printf("put in end_index:");
	scanf("%d",&end_index);

	// �򿪴������ļ�
	FILE *fp = fopen("send_file", "rb");
    if (fp == NULL)
    {
        printf("Failed to open file.\n");
        return -1;
    }

	// ÿ�ζ�ȡ������һ�����ݿ�
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
		load_ethernet_header(buffer); // ������̫��ͷ��
		// �������ݰ�
		pcap_t *handle;
		// ����һ��ָ�� pcap_t �ṹ��ָ�룬���ڹ�������ӿڵĲ�������
		char *device;
		// device�����豸��Ϣ
		char error_buffer[PCAP_ERRBUF_SIZE];
		// error_buffer���������Ϣ
		device = pcap_lookupdev(error_buffer);
		// ���� pcap_lookupdev ����Ĭ�ϵ������豸����������
		if (device == NULL)
		{
			printf("%s\n", error_buffer);
			return -1;
		}
		
		// handle
		handle = pcap_open_live(device, size_of_packet, PCAP_OPENFLAG_PROMISCUOUS, 1, error_buffer);
		// ʹ�� pcap_open_live ��ָ�������豸�Ĳ�����
		if (handle == NULL)
		{
			printf("Open adapter failed..\n");
			return -1;
		}

		if (pcap_sendpacket(handle, (const u_char*)buffer, size_of_packet) != 0)
		// ���� pcap_sendpacket ͨ�������� handle �������ݰ���
		{
			printf("Failed to send packet: %s\n", pcap_geterr(handle));
			pcap_close(handle);
			fclose(fp);
			return -1;
		}
		printf("packet(index: %d) is send successfully\n",chunk_index);
		printf("----------------------\n");
		pcap_close(handle);
		// �������ݰ���С
		size_of_packet = 0;
		chunk_index++;
	}

	// �ر��ļ�
	fclose(fp);
	return 0;
}