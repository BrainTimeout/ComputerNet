#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define BUF_SIZE 1024

void send_file(SOCKET sock, const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }

    fseek(file, 0, SEEK_END);  // Go to the end of the file
    long total_size = ftell(file);  // Get the total size of the file
    fseek(file, 0, SEEK_SET);  // Go back to the beginning of the file

    char buffer[BUF_SIZE];
    size_t bytes_read;
    long bytes_sent = 0;  // To track how much data has been sent

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(sock, buffer, bytes_read, 0);
        bytes_sent += bytes_read;

        // Calculate the progress percentage
        int progress = (int)((double)bytes_sent / total_size * 100);
        printf("Sent %ld bytes (%d%%)\n", bytes_sent, progress);  // Print progress
    }

    fclose(file);
    printf("File transfer completed.\n");
}

int main() {
    WSADATA wsaData;
    SOCKET client_sock;
    struct sockaddr_in server_addr;
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

    printf("Connected to server. Sending file...\n");

    // 发送文件
    send_file(client_sock, "test.avi");

    closesocket(client_sock);
    WSACleanup();
    return 0;
}
