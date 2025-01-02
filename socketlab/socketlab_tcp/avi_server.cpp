#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024

void receive_file(SOCKET sock, const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }

    char buffer[BUF_SIZE];
    int bytes_received;
    long total_received = 0;  // Track the total bytes received

    // Get the total file size (assuming the client sends the file in chunks)
    while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        fwrite(buffer, 1, bytes_received, file);
        total_received += bytes_received;

        // Print progress (for now, no total file size, will show bytes received)
        printf("Received %ld bytes\n", total_received);  // Print progress
    }

    fclose(file);
    printf("File received and saved as %s\n", filename);
}

int main() {
    WSADATA wsaData;
    SOCKET server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    int client_addr_len = sizeof(client_addr);
    unsigned short server_port;

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
    server_addr.sin_addr.s_addr = INADDR_ANY;
    printf("Enter server port: "); // 输入服务器port
    scanf("%hu", &server_port);
    server_addr.sin_port = htons(server_port);

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

    printf("Server is listening on port %hu\n", server_port);

    // 接受客户端连接
    if ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len)) == INVALID_SOCKET) {
        printf("Accept failed\n");
        closesocket(server_sock);
        WSACleanup();
        return 1;
    }

    printf("Client connected: IP = %s, PORT = %d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

    // 接收文件
    receive_file(client_sock, "received_test.avi");

    closesocket(client_sock);
    closesocket(server_sock);
    WSACleanup();
    return 0;
}
