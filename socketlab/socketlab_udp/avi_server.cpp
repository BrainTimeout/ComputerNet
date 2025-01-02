#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define FILE_NAME "received_test.avi"


int main() {
    WSADATA wsaData;
    int server_sockid;
    struct sockaddr_in server_addr, client_addr;
    char buf[1024];
    FILE *file;
    socklen_t client_addr_len = sizeof(client_addr);
    int len;
    char server_ip[INET_ADDRSTRLEN];
    unsigned short server_port;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    printf("Enter host port: ");
    scanf("%hu", &server_port);

    if ((server_sockid = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("Socket creation failed\n");
        WSACleanup();
        return 1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(server_port);

    if (bind(server_sockid, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Bind failed\n");
        closesocket(server_sockid);
        WSACleanup();
        return 1;
    }

    file = fopen(FILE_NAME, "wb");
    if (file == NULL) {
        printf("Error opening file: %s\n", FILE_NAME);
        closesocket(server_sockid);
        WSACleanup();
        return 1;
    }

    printf("Waiting for file...\n");

    // 接收文件大小
    long total_size = 0;
    len = recvfrom(server_sockid, (char*)&total_size, sizeof(total_size), 0, (struct sockaddr *)&client_addr, &client_addr_len);
    if (len <= 0) {
        printf("Failed to receive file size\n");
        fclose(file);
        closesocket(server_sockid);
        WSACleanup();
        return 1;
    }
    total_size = ntohl(total_size); // 转换为主机字节序
    printf("Total file size: %ld bytes\n", total_size);

    long received_size = 0;

    // 接收文件数据并显示进度
    while (received_size < total_size) {
        len = recvfrom(server_sockid, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 0) {
            printf("recvfrom failed\n");
            break;
        }
        fwrite(buf, 1, len, file);
        received_size += len;
        printf("\rProgress: %.2f%%", (double)received_size / total_size * 100);
        fflush(stdout);
    }

    printf("\nFile received successfully and saved as %s\n", FILE_NAME);

    fclose(file);
    closesocket(server_sockid);
    WSACleanup();

    return 0;
}
