#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define MAXSIZE 1024


int main() {
    WSADATA wsaData;
    int server_sockid;
    struct sockaddr_in server_saddr, client_addr;
    char recive_buf[MAXSIZE];
    char send_buf[MAXSIZE];
    socklen_t client_addr_len = sizeof(client_addr);
    int len;
    char server_ip[INET_ADDRSTRLEN];
    unsigned short server_port;

    // 初始化 Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // 创建 UDP 套接字
    if ((server_sockid = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // 获取用户输入的本机端口号
    printf("Enter host port: ");
    scanf("%hu", &server_port);

    // 配置服务器地址
    memset(&server_saddr, 0, sizeof(server_saddr));
    server_saddr.sin_family = AF_INET;
    server_saddr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听所有网络接口
    server_saddr.sin_port = htons(server_port);

    // 绑定地址到套接字
    if (bind(server_sockid, (struct sockaddr *)&server_saddr, sizeof(server_saddr)) < 0) {
        printf("Bind failed\n");
        closesocket(server_sockid);
        WSACleanup();
        return 1;
    }

    // 接收数据
    while (1) {
        memset(recive_buf, 0, sizeof(recive_buf));  // 清空缓冲区

        // 接收客户端的数据
        len = recvfrom(server_sockid, recive_buf, sizeof(recive_buf) - 1, 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 0) {
            printf("recvfrom failed\n");
            break;
        }

        recive_buf[len] = '\0';  // 确保接收到的字符串以空字符结尾

        // 打印客户端信息和接收到的消息
        char client_ip_str[INET_ADDRSTRLEN] = "";
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip_str, sizeof(client_ip_str));
        printf("Client IP: %s, Port: %hu\n", client_ip_str, ntohs(client_addr.sin_port));
        printf("Received message: %s\n", recive_buf); 

        strcpy(send_buf,recive_buf); //复制收到的消息用于返回
        // 将接收到的消息回显给客户端
        if (sendto(server_sockid, send_buf, len, 0, (struct sockaddr *)&client_addr, client_addr_len) < 0) {
            printf("sendto failed\n");
            break;
        }
    }

    // 关闭套接字并清理 Winsock
    closesocket(server_sockid);
    WSACleanup();

    return 0;
}
