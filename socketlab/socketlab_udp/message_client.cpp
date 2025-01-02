#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define MAXSIZE 1024


int main() {
    WSADATA wsaData;
    int client_sockid;
    struct sockaddr_in server_addr, client_addr;
    socklen_t server_addr_len = sizeof(server_addr);
    char recive_buf[MAXSIZE];
    char send_buf[MAXSIZE];
    unsigned short server_port;
    char server_ip[INET_ADDRSTRLEN];
    char local_ip[INET_ADDRSTRLEN];

    // 初始化 Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // 创建 UDP 套接字
    if ((client_sockid = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // 获取用户输入的服务器 IP 地址和端口号
    printf("Enter server IP address: ");
    scanf("%s", server_ip);
    printf("Enter server port: ");
    scanf("%hu", &server_port);

    // 配置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    // 发送消息到服务器
    printf("Enter message to send to server: ");
    scanf("%s", send_buf);
    if (sendto(client_sockid, send_buf, strlen(send_buf), 0, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("sendto failed\n");
        closesocket(client_sockid);
        WSACleanup();
        return 1;
    }

    // 接收来自服务器的响应
    int len = recvfrom(client_sockid, recive_buf, sizeof(recive_buf) - 1, 0, (struct sockaddr *)&server_addr, &server_addr_len);
    if (len < 0) {
        printf("recvfrom failed\n");
        closesocket(client_sockid);
        WSACleanup();
        return 1;
    }
    recive_buf[len] = '\0';  // 确保接收到的字符串以空字符结尾
    // 打印服务器返回的信息
    char server_ip_str[INET_ADDRSTRLEN] = "";
    inet_ntop(AF_INET, &server_addr.sin_addr, server_ip_str, sizeof(server_ip_str));
    printf("Server IP: %s, Port: %hu\n", server_ip_str, ntohs(server_addr.sin_port));
    printf("Received message: %s\n", recive_buf);

    // 关闭套接字并清理 Winsock
    closesocket(client_sockid);
    WSACleanup();

    return 0;
}
