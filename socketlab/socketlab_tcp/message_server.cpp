#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024

int main() {
    WSADATA wsaData;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    char buf[BUF_SIZE];
    unsigned short port;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // 创建服务器套接字
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 监听所有网络接口
    printf("Enter server port: ");// 获取本机端口
    scanf("%hu", &port);
    server_addr.sin_port = htons(port);

    // 绑定端口
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        printf("Bind failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    // 监听连接
    if (listen(server_sock, 5) == SOCKET_ERROR) {
        printf("Listen failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Waiting for connection...\n");

    // 接受客户端连接
    if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) == INVALID_SOCKET) {
        printf("Accept failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Client connected: IP = %s, PORT = %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 双向通信
    int len;
    while ((len = recv(client_sock, buf, BUF_SIZE - 1, 0)) > 0) {
        buf[len] = '\0';
        printf("Received message from client(%s:%d): %s\n",inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port),buf);

        // 回传消息给客户端
        send(client_sock, buf, len, 0);
    }

    printf("Client disconnected\n");

    closesocket(client_sock);
    closesocket(server_sock);
    WSACleanup();
    return 0;
}
