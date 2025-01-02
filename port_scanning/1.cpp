#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT 1000  // 超时设置：1秒
#define START_PORT 1   // 起始扫描端口
#define END_PORT 65535 // 结束扫描端口

// 用于初始化Winsock
int initialize_winsock() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return -1;
    }
    return 0;
}

// 扫描端口
void scan_port(const char *ip, unsigned short port) {
    SOCKET sock;
    struct sockaddr_in server_addr;
    fd_set fdset;
    struct timeval timeout;

    // 创建套接字
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        return;
    }

    // 设置超时
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT * 1000; // 设置1秒超时

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    // 尝试连接
    int result = connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (result == SOCKET_ERROR) {
        // 如果连接失败，检查是否超时
        if (select(0, NULL, &fdset, NULL, &timeout) == 0) {
            printf("Port %hu is closed\n", port);
        }
    } else {
        printf("Port %hu is open\n", port);
    }

    closesocket(sock);
}

// 扫描给定IP的多个端口
void scan_ports(const char *ip) {
    for (unsigned short port = START_PORT; port <= END_PORT; ++port) {
        scan_port(ip, port);
    }
}

int main() {
    char server_ip[INET_ADDRSTRLEN];

    // 初始化Winsock
    if (initialize_winsock() != 0) {
        return 1;
    }

    printf("Enter server IP: ");
    scanf("%s", server_ip);

    printf("Scanning ports on %s...\n", server_ip);

    // 执行端口扫描
    scan_ports(server_ip);

    // 清理资源
    WSACleanup();
    return 0;
}
