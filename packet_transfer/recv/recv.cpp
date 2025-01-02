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
//{ 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } �㲥��ַ

FILE *recived_file; //�����ļ�ָ��

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

	// д���ļ�
    recived_file = fopen("recived_file", "ab");
    //ab:��д
    if (!recived_file) {
        fprintf(stderr, "Failed to create file.\n");
        return -1;
    }

	while(pwbf->index == wbh.need_index){
		fwrite(pwbf->data, 1, pwbf->data_len, recived_file);
		//д������
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
	// ���ݱ���С
    int i, j;

    // �������֡��Ŀ�� MAC ��ַ����ն�����ӿڵ� MAC ��ַƥ�䣬���ն˲Żᴦ���֡��
	// �����accept_dest_mac[0][6]��˵���ǹ㲥������������MAC���ɽ���
	// �����accept_dest_mac[1][6]��˵���Ƿ����Լ�������
	// ������߶����ǣ��˳����λص��������к�������
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
    
    //�ǿɽ��յ�MAC��ַ����ʼ��ӡ�����Ϣ
    printf("----------------------\n");
    
	// iΪ0��˵���ǹ㲥
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
    
    //У�����ȷ��������һ������
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

    mac_string = ethernet_protocol->ether_shost; //Դ�˿�
    printf("MAC source address: %02x:%02x:%02x:%02x:%02x:%02x\n", *mac_string, *(mac_string + 1), *(mac_string + 2), *(mac_string + 3),
        *(mac_string + 4), *(mac_string + 5));
    mac_string = ethernet_protocol->ether_dhost; //Ŀ�Ķ˿�
    printf("MAC destination address: %02x:%02x:%02x:%02x:%02x:%02x\n", *mac_string, *(mac_string + 1), *(mac_string + 2),
        *(mac_string + 3), *(mac_string + 4), *(mac_string + 5));
	
	const unsigned char *payload_start = packet_content + sizeof(struct ethernet_header);
	int this_index = *(int*)payload_start;
	payload_start = payload_start + 4;
    //packet_content:���յ��Ļ�����ָ��
    //��ʼλ�ü���ͷ�ĳ��ȣ������������ݵ���ʼλ��
	
	int data_len  = packet_header->len - sizeof(struct ethernet_header) - 4 - 4;
	//���ݳ��ȵ����ܳ��ȼ�ȥͷ�ĳ����ټ�ȥβ��У�����࣬���ü�ȥindex��4�ֽ�
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
	//���� WinPcap �ṩ�� pcap_findalldevs_ex ������ȡ���п��õ�����������,���ҵ�����������Ϣ�洢�� all_adapters ������
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
	// ���� all_adapters ������ÿ�������������ƺ�������ӡ��������Ϊ�����һ����š�

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
	// ����������λ���û�ѡ���������
    adapter_handle = pcap_open(adapter->name, 65535, PCAP_OPENFLAG_PROMISCUOUS, 5, NULL, error_buffer);
	// ���û�ѡ���������������
    if (adapter_handle == NULL)
    {
        fprintf(stderr, "\n Unable to open adapter: %s\n", adapter->name);
        pcap_freealldevs(all_adapters);
        return -1;
    }

	pcap_loop(adapter_handle, NULL, ethernet_protocol_packet_callback, NULL);
	// ���� pcap_loop ��ʼʹ��adapter_handle����������ݰ�
	// ��������
	// - NULL:�����ƵĲ����������ݰ�
	// - ethernet_protocol_packet_callback���ص�������ÿ����һ�����ݰ�����һ�Ρ�
	// - NULL:���ݸ��ص������Ĳ���
	pcap_close(adapter_handle);
	pcap_freealldevs(all_adapters);
	return 0;

    return 0;
}
