#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define MAXSIZE 1024

// ICMP校验和计算函数
unsigned short calculate_checksum(void *buffer, int length) {
    unsigned short *data = reinterpret_cast<unsigned short *>(buffer);
    unsigned long sum = 0;
    for (int i = 0; i < length / 2; ++i) {
        sum += data[i];
    }
    if (length % 2) {
        sum += *reinterpret_cast<unsigned char *>(data + length / 2);
    }
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    return ~static_cast<unsigned short>(sum);
}

int main() {
    WSADATA wsaData;
    int client_sockid;
    struct sockaddr_in server_addr;
    char send_buf[MAXSIZE];
    char server_ip[INET_ADDRSTRLEN];
    unsigned short server_port;

    // 初始化 Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // 创建原始套接字
    if ((client_sockid = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // 获取用户输入的服务器 IP 地址
    printf("Enter server IP address: ");
    scanf("%s", server_ip);
    
    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // 构造ICMP报文
    memset(send_buf, 0, sizeof(send_buf));
    struct icmp {
        unsigned char type;   // 类型
        unsigned char code;   // 代码
        unsigned short checksum; // 校验和
        unsigned short id;    // 标识符
        unsigned short seq;   // 序列号
    } *icmp_header;
    
    // 填充头部
    icmp_header = reinterpret_cast<icmp *>(send_buf);
    icmp_header->type = 8;  // ECHO请求
    icmp_header->code = 0;
    icmp_header->id = htons(GetCurrentProcessId() & 0xFFFF); // 进程ID作为标识符
    icmp_header->seq = htons(0);                            // 序列号
    icmp_header->checksum = 0;  // 校验和初始为0

    // 填充数据
    char *data = send_buf + sizeof(struct icmp); 
    printf("Enter message to send to server: ");
    scanf("%s", data);

    // 计算并填充校验和
    icmp_header->checksum = calculate_checksum(send_buf, sizeof(struct icmp) + strlen(data));
    printf("Checksum: 0x%04X\n", ntohs(icmp_header->checksum));
    // 发送 ICMP 数据包
    int packet_size = sizeof(struct icmp) + strlen(data); // 正确计算数据包大小
    if (sendto(client_sockid, send_buf, packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("sendto failed: %d\n", WSAGetLastError());
        closesocket(client_sockid);
        WSACleanup();
        return 1;
    }

    printf("Send success\n");

    // 关闭套接字并清理 Winsock
    closesocket(client_sockid);
    WSACleanup();

    return 0;
}
