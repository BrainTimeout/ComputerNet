#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024

int main() {
    WSADATA wsaData;
    SOCKET client_sock;
    struct sockaddr_in server_addr;
    char buf[BUF_SIZE];
    char server_ip[INET_ADDRSTRLEN];
    unsigned short server_port;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // 创建客户端套接字
    if ((client_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    printf("Enter server IP: "); // 输入服务器ip
    scanf("%s", server_ip);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    printf("Enter server port: "); // 输入服务器port
    scanf("%hu", &server_port);
    server_addr.sin_port = htons(server_port);

    // 连接服务器
    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Connection failed\n");
        closesocket(client_sock);
        WSACleanup();
        return 1;
    }

    printf("Connected to server at %s:%hu\n", server_ip, server_port);
    printf("Enter messages (type 'exit' to quit):\n");

    // 双向通信
    while (1) {
        printf("Message: ");
        scanf(" %[^\n]", buf);

        if (strcmp(buf, "exit") == 0) {
            break;
        }

        // 发送消息到服务器
        send(client_sock, buf, strlen(buf), 0);

        // 接收服务器的回传消息
        int len = recv(client_sock, buf, BUF_SIZE - 1, 0);
        if (len > 0) {
            buf[len] = '\0';
            printf("Received from server (%s:%hu): %s\n", server_ip, server_port, buf);
        } else {
            printf("Connection lost.\n");
            break;
        }
    }

    closesocket(client_sock);
    WSACleanup();
    return 0;
}
