# 使用winpcap发送和接收文件

## 用法

- `send.exe`

  在`send.exe`所在的文件夹下，创建一个`send_file`文件，该文件没有后缀，是待发送的文件。运行`send`，会要求输入两个整数，分别是数据包开始序号和结束序号（由于文件过大，将其切为多个数据报并标上序号），之后`send.exe`就会将两个序号间的数据报发给接收端

- `recv.exe`

  在`recv.exe`所在的文件夹下，运行结束后会产生一个`recv_file`文件，该文件没有后缀，是接收到的文件。运行`recv`，会要求输入一个整数，决定了使用什么类型的mac接收数据报。

  在`recv.exe`运行过程中，会持续接收到数据报。当接收到的数据包目的地址为广播地址时，会多显示一行`It's broadcasted.`，如果接收到的文件有误，则会打印`The data has been changed`，说明此时数据包校验码对不上，该数据报不会保存。

  `recv.exe`设计了一个对不同数据报处理的机制，其中有一个need_index，是`recv.exe`已经收的数据报的下一个数据报。
  
  1. 如果接收到的数据包的序号正好是 `need_index`，则直接保存数据包并更新 `need_index`，然后打印信息 `packet(index:%d) is saved successfully`。
  2. 如果接收到的序号小于 `need_index`，说明该数据包已被接收过，程序会打印信息 `packet before (index:%d) is recived`，并跳过该数据包。
  3. 如果接收到的数据包的序号大于 `need_index`，则说明数据包丢失或顺序错乱，程序会打印信息 `packet(index:%d) is recived but not saved`，将这个数据报的数据存在一个链表中，等待`need_index`的数据报到达。当该数据报到达后，会将从该数据报开始的保存下来的具有连续序号的数据报存入`recv_file`，直到遇到下一个缺失的数据报，将need_index设置为该数据报的序号。

## 代码

### 发送文件

`unsigned char dest_mac`：接收端MAC地址
`unsigned char source_mac` ：发送端MAC地址

```
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
```

### 接收文件

`unsigned char accept_dest_mac`:包含两个地址，一个是广播地址，另一个是本机的一个MAC地址，如果接收到的数据报目标地址不是该地址，说明不是发给本机，那么就丢掉。

```
#include<stdio.h>
#include<stdlib.h>


#define HAVE_REMOTE
#include<pcap.h>
#include<WinSock2.h>

#pragma warning(disable:4996)
#pragma comment(lib,"packet.lib")
#pragma comment(lib,"wpcap.lib")
#pragma comment(lib,"ws2_32.lib")

// Ethernet protocol header format
struct ethernet_header
{
    unsigned char ether_dhost[6]; // destination MAC
    unsigned char ether_shost[6]; // source MAC
    u_int16_t ether_type;
};

unsigned char accept_dest_mac[2][6] = { { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }, { 0x00, 0x50, 0x56, 0xC0, 0x00, 0x08 } };
//{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } 广播地址

FILE *recived_file; //接收文件指针

struct write_buffer_head
{
	int need_index;
	int now_length;
	struct write_buffer* next;
};

struct write_buffer{
	struct write_buffer* next;
	const unsigned char *data;
	int data_len;
	int index;
};

struct write_buffer_head wbh = {0,0,NULL};

u_int32_t crc32_table[256];

// Generate CRC32 table
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

// Calculate CRC32 checksum
u_int32_t calculate_crc(unsigned char *buffer, int len)
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


int write_data(int index,const unsigned char *payload_start,int data_len){
	if(wbh.now_length>=10 && index!=wbh.need_index){
		printf("packet(index:%d) may be lost.\n",wbh.need_index);
		return -1;
	}

    int now_index = index;
	struct write_buffer* pwbf = (struct write_buffer*)malloc(sizeof(struct write_buffer));
	pwbf->data_len = data_len;
	pwbf->index = index;
	pwbf->next = wbh.next;
	pwbf->data = (const unsigned char *)malloc(pwbf->data_len * sizeof(unsigned char));
	memcpy((void *)(pwbf->data), payload_start, data_len);	
	wbh.next = pwbf;
	wbh.now_length++;
	struct write_buffer** p1 = &(wbh.next);
	
	if(pwbf->index < wbh.need_index){
		printf("packet before (index:%d) is recived.\n",wbh.need_index);
		return -1;
	}

	if(pwbf->index > wbh.need_index){
		while(pwbf->next != NULL){
			if(pwbf->index<(pwbf->next)->index) break;
			*p1 = pwbf->next;
			pwbf->next = (*p1)->next;
			(*p1)->next = pwbf;
			p1 = &((*p1)->next);
		}
		printf("packet(index:%d) is recived but not saved.\n",pwbf->index);
		printf("packet(index:%d) is needed.\n",wbh.need_index);
		return 1;
	}

	// 写入文件
    recived_file = fopen("recived_file", "ab");
    //ab:续写
    if (!recived_file) {
        fprintf(stderr, "Failed to create file.\n");
        return -1;
    }

	while(pwbf->index == wbh.need_index){
		fwrite(pwbf->data, 1, pwbf->data_len, recived_file);
		//写入数据
		printf("packet(index:%d) is saved successfully.\n",pwbf->index);
		wbh.need_index++;
		wbh.now_length--;
		wbh.next = pwbf->next;
		free((void*)(pwbf->data));
		free((void*)pwbf);
		pwbf = wbh.next;
		if(pwbf == NULL) break;
	}
	fclose(recived_file);
	printf("packet(index:%d) is needed.\n",wbh.need_index);
	return 1;
}

// Ethernet protocol analysis callback function
void ethernet_protocol_packet_callback(u_char *argument, const struct pcap_pkthdr *packet_header, const u_char *packet_content)
{
    u_short ethernet_type;
    struct ethernet_header *ethernet_protocol;
    unsigned char *mac_string;
    static int packet_number = 1;
    ethernet_protocol = (struct ethernet_header*)packet_content;
    int len = packet_header->len;
	// 数据报大小
    int i, j;

    // 如果数据帧的目标 MAC 地址与接收端网络接口的 MAC 地址匹配，接收端才会处理该帧。
	// 如果是accept_dest_mac[0][6]，说明是广播，即本机所有MAC都可接受
	// 如果是accept_dest_mac[1][6]，说明是发给自己的数据
	// 如果两者都不是，退出本次回调，不进行后续操作
    int flag = 2;
    for (i = 0; i < 2; i++)
    {
        flag = 2;
        for (j = 0; j < 6; j++)
        {
            if (ethernet_protocol->ether_dhost[j] == accept_dest_mac[i][j])
                continue;
            else
            {
                flag = i;
                break;
            }
        }
        if (flag != 2) continue;
        else
            break;
    }

    if (flag != 2)
    {
        return;
    }
    
    //是可接收的MAC地址，开始打印相关信息
    printf("----------------------\n");
    
	// i为0，说明是广播
    if (i == 0)
    {	
        printf("It's broadcasted.\n");
    }

    // Check if data is changed or not
    u_int32_t crc = calculate_crc((unsigned char*)(packet_content + sizeof(ethernet_header)), len - 4 - sizeof(ethernet_header));
    u_int32_t received_crc = *(u_int32_t *)(packet_content + len - 4);
    printf("Received CRC: 0x%08x\n", received_crc);
    printf("Calculated CRC: 0x%08x\n", crc);
    
    if (crc != *((u_int32_t*)(packet_content + len - 4)))
    {
        printf("The data has been changed.\n");
        printf("----------------------\n");
        return;
    }
    
    //校验和正确，进行下一步操作
    printf("Captured %d packet\n", packet_number);
    printf("Capture time: %d\n", packet_header->ts.tv_sec);
    printf("Packet length: %d\n", packet_header->len);
    printf("~~~~~Ethernet protocol~~~~~~\n");

    ethernet_type = ethernet_protocol->ether_type;
    printf("Ethernet type: %04x\n", ethernet_type);
    switch (ethernet_type)
    {
    case 0x0800: printf("Upper layer protocol: IPV4\n"); break;
    case 0x0806: printf("Upper layer protocol: ARP\n"); break;
    case 0x8035: printf("Upper layer protocol: RARP\n"); break;
    case 0x814c: printf("Upper layer protocol: SNMP\n"); break;
    case 0x8137: printf("Upper layer protocol: IPX\n"); break;
    case 0x86dd: printf("Upper layer protocol: IPV6\n"); break;
    case 0x880b: printf("Upper layer protocol: PPP\n"); break;
    default:
        break;
    }

    mac_string = ethernet_protocol->ether_shost; //源端口
    printf("MAC source address: %02x:%02x:%02x:%02x:%02x:%02x\n", *mac_string, *(mac_string + 1), *(mac_string + 2), *(mac_string + 3),
        *(mac_string + 4), *(mac_string + 5));
    mac_string = ethernet_protocol->ether_dhost; //目的端口
    printf("MAC destination address: %02x:%02x:%02x:%02x:%02x:%02x\n", *mac_string, *(mac_string + 1), *(mac_string + 2),
        *(mac_string + 3), *(mac_string + 4), *(mac_string + 5));
	
	const unsigned char *payload_start = packet_content + sizeof(struct ethernet_header);
	int this_index = *(int*)payload_start;
	payload_start = payload_start + 4;
    //packet_content:接收到的缓冲区指针
    //开始位置加上头的长度，即是主体数据的起始位置
	
	int data_len  = packet_header->len - sizeof(struct ethernet_header) - 4 - 4;
	//数据长度等于总长度减去头的长度再减去尾部校验冗余，还得减去index的4字节
	int write_result;
	write_result = write_data(this_index,payload_start,data_len);
	

    printf("----------------------\n");
    packet_number++;
}

int main()
{
    generate_crc32_table();

    pcap_if_t *all_adapters;
    pcap_if_t *adapter;
    pcap_t *adapter_handle;
    char error_buffer[PCAP_ERRBUF_SIZE];

    if (pcap_findalldevs_ex(PCAP_SRC_IF_STRING, NULL, &all_adapters, error_buffer) == -1)
	//调用 WinPcap 提供的 pcap_findalldevs_ex 函数获取所有可用的网络适配器,将找到的适配器信息存储到 all_adapters 链表中
    {
        fprintf(stderr, "Error in findalldevs_ex function: %s\n", error_buffer);
        return -1;
    }
    if (all_adapters == NULL)
    {
        printf("\nNo adapters found! Make sure WinPcap is installed!!!\n");
        return 0;
    }

    int id = 1;
    for (adapter = all_adapters; adapter != NULL; adapter = adapter->next)
    {
        printf("\n%d.%s\n", id++, adapter->name);
        printf("--- %s\n", adapter->description);
    }
    printf("\n");
	// 遍历 all_adapters 链表，将每个适配器的名称和描述打印出来，并为其分配一个编号。

    int adapter_id;
    printf("Enter the adapter id between 1 and %d: ", id - 1);
    scanf("%d", &adapter_id);
    if (adapter_id < 1 || adapter_id > id - 1)
    {
        printf("\n Adapter id out of range.\n");
        pcap_freealldevs(all_adapters);
        return -1;
    }

    adapter = all_adapters;
    for (id = 1; id < adapter_id; id++)
    {
        adapter = adapter->next;
    }
	// 遍历链表，定位到用户选择的适配器
    adapter_handle = pcap_open(adapter->name, 65535, PCAP_OPENFLAG_PROMISCUOUS, 5, NULL, error_buffer);
	// 打开用户选择的网络适配器。
    if (adapter_handle == NULL)
    {
        fprintf(stderr, "\n Unable to open adapter: %s\n", adapter->name);
        pcap_freealldevs(all_adapters);
        return -1;
    }

	pcap_loop(adapter_handle, NULL, ethernet_protocol_packet_callback, NULL);
	// 调用 pcap_loop 开始使用adapter_handle句柄捕获数据包
	// 持续监听
	// - NULL:无限制的捕获所有数据包
	// - ethernet_protocol_packet_callback：回调函数，每捕获一个数据包调用一次。
	// - NULL:传递给回调函数的参数
	pcap_close(adapter_handle);
	pcap_freealldevs(all_adapters);
	return 0;

    return 0;
}

```

其中，使用一个链表维护收到的数据报：

```
struct write_buffer_head
{
	int need_index;
	int now_length;
	struct write_buffer* next;
};

struct write_buffer{
	struct write_buffer* next;
	const unsigned char *data;
	int data_len;
	int index;
};

struct write_buffer_head wbh = {0,0,NULL};
```

当当前收到的数据包前有还有没收到的数据包，就先不存储这个，而是放到这个链表中，当收到那个数据包时，再一并存储。

```
while(pwbf->next != NULL){
			if(pwbf->index<(pwbf->next)->index) break;
			*p1 = pwbf->next;
			pwbf->next = (*p1)->next;
			(*p1)->next = pwbf;
			p1 = &((*p1)->next);
		}
```

这串代码用于不断交换链表中的两处位置，直到序号从小到大。
