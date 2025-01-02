#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define FILE_NAME "test.avi"

int main() {
    WSADATA wsaData;
    int client_sockid;
    struct sockaddr_in server_addr;
    char buf[1024];
    FILE *file;
    unsigned short server_port;
    char server_ip[INET_ADDRSTRLEN];
    char client_ip[INET_ADDRSTRLEN];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    printf("Enter server IP: ");
    scanf("%s", server_ip);

    printf("Enter server port: ");
    scanf("%hu", &server_port);

    if ((client_sockid = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);
    server_addr.sin_port = htons(server_port);

    file = fopen(FILE_NAME, "rb");
    if (file == NULL) {
        printf("Error opening file: %s\n", FILE_NAME);
        closesocket(client_sockid);
        WSACleanup();
        return 1;
    }

    // 发送文件大小
    fseek(file, 0, SEEK_END);
    long total_size = ftell(file);
    rewind(file);
    long network_total_size = htonl(total_size); // 转换为网络字节序
    sendto(client_sockid, (char *)&network_total_size, sizeof(network_total_size), 0, (struct sockaddr *)&server_addr, sizeof(server_addr));

    printf("Total file size: %ld bytes\n", total_size);

    // 发送文件数据并显示进度
    long sent_size = 0;
    int len;
    while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
        sendto(client_sockid, buf, len, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        sent_size += len;
        printf("\rProgress: %.2f%%", (double)sent_size / total_size * 100);
        fflush(stdout);
    }

    printf("\nFile sent successfully\n");

    fclose(file);
    closesocket(client_sockid);
    WSACleanup();

    return 0;
}
