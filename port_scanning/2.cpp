#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define TIMEOUT 1000  // 超时设置：1秒
#define START_PORT 1   // 起始扫描端口
#define END_PORT 1024 // 结束扫描端口
#define NUM_PROCESSES 16 // 每次并行启动的进程数
#define EACH_SCAN_NUM 64

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

struct scanp{
    const char *ip;
    int start;
    int end;
};

// 执行扫描任务的函数
DWORD WINAPI scan_ports_in_process(LPVOID lpParam) {
    SOCKET sock;
    struct sockaddr_in server_addr;
    fd_set fdset;
    struct timeval timeout;

    struct scanp *pscpin = (struct scanp*)lpParam;
    char *ip = (char*)(pscpin->ip);
    int pstart = pscpin->start;
    int pend = pscpin->end;
    for (unsigned short port = pstart; port <= pend; ++port) {
        // 创建套接字
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == INVALID_SOCKET) {
            printf("Socket creation failed\n");
            exit(0);
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
        }else {
        printf("Port %hu is open\n", port);
    }

    closesocket(sock);
    }
    return 0;
}

// 启动多个进程来扫描端口
void scan_ports_multithreaded(const char *ip) {
    HANDLE processes[NUM_PROCESSES];
    DWORD process_id[NUM_PROCESSES];
    int PROStart = 0;  
    struct scanp scpin[NUM_PROCESSES];
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        scpin[i].ip = ip;
        scpin[i].start = PROStart;
        scpin[i].end = PROStart + EACH_SCAN_NUM;
        processes[i] = CreateThread(
            NULL, 
            0, 
            scan_ports_in_process, 
            (LPVOID)(&scpin[i]), 
            0, 
            &process_id[i]
        );
        if (processes[i] == NULL) {
            printf("CreateThread failed\n");
            return;
        }
        PROStart += EACH_SCAN_NUM;
    }

    // 等待所有进程完成
    WaitForMultipleObjects(NUM_PROCESSES, processes, TRUE, INFINITE);

    // 关闭进程句柄
    for (int i = 0; i < NUM_PROCESSES; ++i) {
        CloseHandle(processes[i]);
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

    // 执行并行端口扫描
    scan_ports_multithreaded(server_ip);

    // 清理资源
    WSACleanup();
    return 0;
}
