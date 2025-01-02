#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstring>
#include <cstdlib>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <fstream>

#pragma comment(lib, "ws2_32.lib")

#define MAX_SIZE 4096

char server_ip[INET_ADDRSTRLEN];
char CmdBuf[MAX_SIZE];

SOCKET SocketControl;

std::queue<std::string> msgQueue; // 用于存储接收到的消息
std::mutex mtx;                   // 用于保护消息队列的互斥量
std::condition_variable cv;       // 用于线程同步

std::mutex InOutMtx; // 控制输入输出的锁

// 接收FTP服务器应答并放入队列
void RecvReplyThread()
{
    char replyMsg[MAX_SIZE];
    int nRecv = 0;

    while (true)
    {
        memset(replyMsg, 0, MAX_SIZE);
        nRecv = recv(SocketControl, replyMsg, MAX_SIZE, 0);
        if (nRecv == SOCKET_ERROR)
        {
            std::cout << "\nSocket is closed!" << std::endl;
            closesocket(SocketControl);
            break;
        }

        if (nRecv > 0)
        {
            std::lock_guard<std::mutex> lock(mtx); // 锁住队列

            // 按照\r\n分隔多个响应
            char *token = strtok(replyMsg, "\r\n");
            while (token != NULL)
            {
                msgQueue.push(std::string(token)); // 将每个响应加入队列
                token = strtok(NULL, "\r\n");      // 获取下一个响应
            }

            cv.notify_one(); // 通知等待的线程
        }
    }
}

// 从队列中读取消息，如果队列为空则等待
bool GetReplyFromQueue(std::string &replyMsg)
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []()
            { return !msgQueue.empty(); }); // 等待直到队列中有数据
    replyMsg = msgQueue.front();
    msgQueue.pop();
    return true;
}

// 向FTP服务器发送命令
bool SendCommand(char *Command)
{
    int nSend = send(SocketControl, Command, strlen(Command), 0);
    if (nSend == SOCKET_ERROR)
    {
        std::cout << "SocketControl create error: " << WSAGetLastError() << std::endl;
        return false;
    }
    return true;
}

// 建立数据连接
bool Data_Connect(SOCKET &SocketData)
{
    char Command[MAX_SIZE];
    int nReplyCode;
    std::string ReplyMsg; // 使用 std::string 代替 char[] 数组
    memset(Command, 0, MAX_SIZE);
    memcpy(Command, "PASV", strlen("PASV"));
    memcpy(Command + strlen("PASV"), "\r\n", 2);
    if (!SendCommand(Command))
        return false;

    // 从队列获取应答信息
    if (GetReplyFromQueue(ReplyMsg))
    {
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);
        if (nReplyCode != 227)
        {
            std::cout << "PASV command response error!" << std::endl;
            closesocket(SocketControl);
            return false;
        }
    }

    // 解析PASV响应
    char *part[6];
    char *token = strtok(&ReplyMsg[0], "("); // 使用 std::string 转换为 C 风格字符串处理
    if (token)
    {
        for (int i = 0; i < 5; i++)
        {
            part[i] = strtok(NULL, ",");
            if (!part[i])
                return false;
        }
        part[5] = strtok(NULL, ")");
        if (!part[5])
            return false;
    }
    else
        return false;

    unsigned short serverPort = (unsigned short)((atoi(part[4]) << 8) + atoi(part[5]));

    SocketData = socket(AF_INET, SOCK_STREAM, 0);
    if (SocketData == INVALID_SOCKET)
    {
        std::cout << "Data socket create error: " << WSAGetLastError() << std::endl;
        return false;
    }

    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(serverPort);
    server_addr.sin_addr.S_un.S_addr = inet_addr(server_ip);

    int nConnect = connect(SocketData, (sockaddr *)&server_addr, sizeof(server_addr));
    if (nConnect == SOCKET_ERROR)
    {
        std::cout << "Create data TCP connection error: " << WSAGetLastError() << std::endl;
        return false;
    }
    std::cout << "Data connection established to " << server_ip << ":" << serverPort << std::endl;
    return true;
}

// 接收数据并保存或输出的线程函数
void Download_Data_Thread(SOCKET socketData, const char *command, bool saveToFile, const std::string &savePath)
{
    char Buffer[MAX_SIZE];
    int nRecv = 0;

    if (saveToFile)
    {
        // 如果需要保存到文件
        std::ofstream outFile(savePath, std::ios::binary);
        if (outFile)
        {
            while ((nRecv = recv(socketData, Buffer, sizeof(Buffer), 0)) > 0)
            {
                outFile.write(Buffer, nRecv);
            }
            closesocket(socketData);
            InOutMtx.lock();
            std::cout << "Successfully Save" << std::endl;
        }
        else
        {
            closesocket(socketData);
            InOutMtx.lock();
            std::cout << "Save Path: " << savePath << std::endl;
            std::cout << "Failed to open file for saving" << std::endl;
            std::cout << "Force close the datasocket" << std::endl;
        }

        std::string ReplyMsg;
        if (GetReplyFromQueue(ReplyMsg))
        {
            int nReplyCode;
            // 解析响应码
            sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);

            std::cout << ReplyMsg << std::endl;
        }
        InOutMtx.unlock();
        outFile.close();
    }
    else
    {
        // 否则只打印数据
        InOutMtx.lock();
        while ((nRecv = recv(socketData, Buffer, sizeof(Buffer), 0)) > 0)
        {
            std::cout << Buffer;
        }
        std::cout << std::endl;

        closesocket(socketData);

        std::string ReplyMsg;
        if (GetReplyFromQueue(ReplyMsg))
        {
            std::cout << ReplyMsg << std::endl;
        }
        InOutMtx.unlock();
    }
}

// 数据更新线程函数：将本地文件上传至FTP服务器
void Upload_Data_Thread(SOCKET SocketData, FILE *file)
{
    char buffer[1024];
    int bytesRead;
    int totalBytesSent = 0;   // 记录总共发送的字节数
    fseek(file, 0, SEEK_END); // 获取文件大小
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // 重置文件指针到文件开始位置

    // 读取本地文件并通过数据连接上传
    InOutMtx.lock();
    std::cout << "Starting file upload..." << std::endl;
    InOutMtx.unlock();

    while ((bytesRead = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        int bytesSent = send(SocketData, buffer, bytesRead, 0);
        if (bytesSent == SOCKET_ERROR)
        {
            closesocket(SocketData);
            fclose(file);
            InOutMtx.lock();
            std::cout << "Error sending data. Upload failed." << std::endl;
            InOutMtx.unlock();
            return;
        }

        totalBytesSent += bytesSent;

        // 显示上传进度
        InOutMtx.lock();
        std::cout << "Uploaded " << totalBytesSent << " of " << fileSize << " bytes ("
                  << (double)totalBytesSent / fileSize * 100 << "%)" << std::endl;
        InOutMtx.unlock();
    }

    // 上传完成后关闭文件和数据连接
    fclose(file);
    closesocket(SocketData);

    InOutMtx.lock();
    // 上传完成，等待服务器确认
    std::string ReplyMsg;
    if (GetReplyFromQueue(ReplyMsg))
        std::cout << ReplyMsg << std::endl;
    InOutMtx.unlock();
}

void Stor_Command()
{
    char localFilePath[MAX_PATH];
    char remoteFilePath[MAX_PATH];

    // 提示用户输入本地文件路径和远程路径
    InOutMtx.lock();
    std::cout << "Enter the local file path to upload: ";
    std::cin.getline(localFilePath, MAX_PATH);

    std::cout << "Enter the remote file path on the FTP server: ";
    std::cin.getline(remoteFilePath, MAX_PATH);

    // 打开本地文件
    FILE *file = fopen(localFilePath, "rb");
    if (!file)
    {
        std::cout << "Failed to open local file: " << localFilePath << std::endl;
        InOutMtx.unlock();
        return;
    }

    // 尝试建立数据连接
    SOCKET SocketData;
    if (!Data_Connect(SocketData))
    {
        std::cout << "Failed to establish data connection for STOR command" << std::endl;
        InOutMtx.unlock();
        fclose(file);
        return;
    }

    // 发送 STOR 命令到服务器
    char command[MAX_SIZE];
    snprintf(command, sizeof(command), "STOR %s\r\n", remoteFilePath);
    if (send(SocketControl, command, strlen(command), 0) == SOCKET_ERROR)
    {
        std::cout << "Failed to send STOR command. Error: " << WSAGetLastError() << std::endl;
        InOutMtx.unlock();
        fclose(file);
        return;
    }

    // 获取服务器响应
    std::string replyMsg;
    if (GetReplyFromQueue(replyMsg))
    {
        int replyCode;
        sscanf(replyMsg.c_str(), "%d", &replyCode);

        // 检查响应码，150 表示准备接收数据
        if (replyCode == 150)
        {
            std::cout << replyMsg << std::endl;
            InOutMtx.unlock();
            // 启动上传线程
            std::thread uploadThread(Upload_Data_Thread, SocketData, file);
            uploadThread.detach(); // 使得线程独立执行
        }
        else
        {
            std::cout << "STOR command failed: " << replyMsg << std::endl;
            InOutMtx.unlock();
        }
    }
}

void Pwd_Command()
{
    char Command[MAX_SIZE] = "PWD\r\n";
    // 发送PWD命令
    if (!SendCommand(Command))
    {
        std::lock_guard<std::mutex> lock(InOutMtx);
        return;
    }

    std::string ReplyMsg;
    // 获取服务器的响应
    if (GetReplyFromQueue(ReplyMsg))
    {
        int nReplyCode;
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);
        std::lock_guard<std::mutex> lock(InOutMtx);
        // 检查响应是否是257（表示当前目录）
        if (nReplyCode == 257)
        {
            // 提取目录路径
            size_t startPos = ReplyMsg.find('"');
            size_t endPos = ReplyMsg.find('"', startPos + 1);

            if (startPos != std::string::npos && endPos != std::string::npos)
            {
                std::string currentDir = ReplyMsg.substr(startPos + 1, endPos - startPos - 1);
                std::cout << "Current directory: " << currentDir << std::endl;
            }
            else
            {
                std::cout << "Failed to parse the current directory from response." << std::endl;
            }
        }
        else
        {
            std::cout << "PWD command response error: " << ReplyMsg << std::endl;
        }
    }
}

void List_Command()
{
    SOCKET SocketData;
    char Command[MAX_SIZE] = "LIST\r\n";

    std::lock_guard<std::mutex> lock(InOutMtx);
    // 尝试建立数据连接
    if (!Data_Connect(SocketData))
    {
        std::cout << "Failed to establish data connection for LIST command" << std::endl;
        return;
    }

    // 发送LIST命令
    if (!SendCommand(Command))
        return;

    std::string ReplyMsg;
    // 获取服务器的响应
    if (GetReplyFromQueue(ReplyMsg))
    {
        int nReplyCode;
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);

        // 检查响应是否是150（表示准备接收数据）
        if (nReplyCode == 150)
        {
            std::cout << ReplyMsg << std::endl;
            // 在新的线程中接收数据并打印
            std::thread data_thread(Download_Data_Thread, SocketData, "LIST", false, "");
            data_thread.detach();
        }
        else
        {
            std::cout << "LIST command response error!" << std::endl;
        }
    }
}

// RETR_Command 函数，从服务器加载文件到本地
void Retr_Command()
{
    // 数据连接
    SOCKET SocketData;
    char Command[MAX_SIZE];
    char filename[MAX_SIZE];

    // 询问用户输入文件名
    std::lock_guard<std::mutex> lock(InOutMtx);
    std::cout << "Enter the filename to retrieve: ";
    std::cin >> filename;

    // 格式化RETR命令
    snprintf(Command, sizeof(Command), "RETR %s\r\n", filename);

    // 尝试建立数据连接
    if (!Data_Connect(SocketData))
    {
        std::cout << "Failed to establish data connection for RETR command" << std::endl;
        return;
    }

    // 发送RETR命令
    if (!SendCommand(Command))
        return;

    std::string ReplyMsg;
    // 获取服务器的响应
    if (GetReplyFromQueue(ReplyMsg))
    {
        int nReplyCode;
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);

        // 检查响应是否是150（表示准备接收数据）
        if (nReplyCode == 150)
        {
            std::cout << ReplyMsg << std::endl;

            // 询问用户是否保存文件
            char saveChoice;
            std::cout << "Do you want to save the file? (y/n): ";
            std::cin >> saveChoice;

            if (saveChoice == 'y' || saveChoice == 'Y')
            {
                // 询问用户输入保存文件的路径和文件名
                std::string savePath;
                std::cout << "Enter the full save path (e.g., C:\\path\\to\\file.txt): ";
                std::cin.ignore(); // 清空输入缓冲区
                std::getline(std::cin, savePath);

                // 在新的线程中接收数据并保存文件
                std::thread data_thread(Download_Data_Thread, SocketData, "RETR", true, savePath);
                data_thread.detach();
            }
            else
            {
                // 如果不保存，则只打印数据
                std::thread data_thread(Download_Data_Thread, SocketData, "RETR", false, "");
                data_thread.detach();
            }
        }
        else
        {
            std::cout << ReplyMsg << std::endl;
        }
    }
}

void Cwd_Command()
{
    char targetDir[MAX_PATH];
    char Command[MAX_SIZE];

    // 提示用户输入目标目录
    InOutMtx.lock();
    std::cout << "Enter the target directory path to change to: ";
    std::cin.getline(targetDir, MAX_PATH);
    InOutMtx.unlock();

    // 格式化CWD命令
    snprintf(Command, sizeof(Command), "CWD %s\r\n", targetDir);

    // 发送CWD命令
    if (!SendCommand(Command))
    {
        InOutMtx.lock();
        std::cout << "Failed to send CWD command." << std::endl;
        InOutMtx.unlock();
        return;
    }

    // 等待服务器确认
    std::string ReplyMsg;
    if (GetReplyFromQueue(ReplyMsg))
        std::cout << ReplyMsg << std::endl;
}

void Quit_Command()
{
    char Command[MAX_SIZE] = "QUIT\r\n";

    // 发送QUIT命令
    if (!SendCommand(Command))
    {
        InOutMtx.lock();
        std::cout << "Failed to send QUIT command." << std::endl;
        InOutMtx.unlock();
        return;
    }

    std::string ReplyMsg;
    // 获取服务器的响应
    if (GetReplyFromQueue(ReplyMsg))
    {
        int nReplyCode;
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);

        // 检查响应是否是221（表示退出成功）
        if (nReplyCode == 221)
        {
            InOutMtx.lock();
            std::cout << "Server closed the connection. Exiting..." << std::endl;
            InOutMtx.unlock();

            // 关闭控制连接
            closesocket(SocketControl);
            WSACleanup(); // 清理Winsock库
            exit(0);      // 退出程序
        }
        else
        {
            InOutMtx.lock();
            std::cout << "QUIT command response error: " << ReplyMsg << std::endl;
            InOutMtx.unlock();
        }
    }
}

void Help_Command()
{
    InOutMtx.lock();
    std::cout << "FTP Client Help:" << std::endl;
    std::cout << "=============================" << std::endl;

    // List Command
    std::cout << "1. `list` : List files and directories in the current directory on the FTP server." << std::endl;
    std::cout << "   Usage: `list`" << std::endl;
    std::cout << "   Description: This command retrieves a listing of the files and directories in the current working directory of the FTP server." << std::endl;

    // Stor Command
    std::cout << "\n2. `stor` : Upload a file to the FTP server." << std::endl;
    std::cout << "   Usage: `stor <local_file_path> <remote_file_path>`" << std::endl;
    std::cout << "   Description: This command uploads a file from your local system to the specified path on the FTP server." << std::endl;
    std::cout << "   Example: `stor C:\\Users\\user\\file.txt /remote/path/file.txt`" << std::endl;

    // Retr Command
    std::cout << "\n3. `retr` : Download a file from the FTP server." << std::endl;
    std::cout << "   Usage: `retr <remote_file_path>`" << std::endl;
    std::cout << "   Description: This command downloads a file from the FTP server to your local machine." << std::endl;
    std::cout << "   Example: `retr /remote/path/file.txt`" << std::endl;

    // Pwd Command
    std::cout << "\n4. `pwd` : Show the current working directory on the FTP server." << std::endl;
    std::cout << "   Usage: `pwd`" << std::endl;
    std::cout << "   Description: This command displays the current working directory on the FTP server." << std::endl;

    // Cwd Command
    std::cout << "\n5. `cwd` : Change the current working directory on the FTP server." << std::endl;
    std::cout << "   Usage: `cwd <target_directory>`" << std::endl;
    std::cout << "   Description: This command changes the current working directory on the FTP server." << std::endl;
    std::cout << "   Example: `cwd /new/directory`" << std::endl;

    // Quit Command
    std::cout << "\n6. `quit` : Exit the FTP client and close the connection." << std::endl;
    std::cout << "   Usage: `quit`" << std::endl;
    std::cout << "   Description: This command terminates the FTP session and closes the connection with the server." << std::endl;

    std::cout << "\n=============================" << std::endl;
    std::cout << "To execute a command, simply type the command name." << std::endl;
    std::cout << "For example: `list`, `stor`, `retr`, etc." << std::endl;
    std::cout << "=============================" << std::endl;

    InOutMtx.unlock();
}

// 判断标准输入是否有可用的输入
bool kbhit()
{
    DWORD dwNumberOfEvents = 0;
    INPUT_RECORD ir;
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);

    // 使用 PeekConsoleInput 来查看输入缓冲区是否有输入
    if (hStdin == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // 检查输入缓冲区是否有可用事件
    if (PeekConsoleInput(hStdin, &ir, 1, &dwNumberOfEvents))
    {
        return dwNumberOfEvents > 0;
    }
    return false;
}

void Command_Thread()
{
    while (true)
    {
        if (kbhit()) // 如果键盘有输入
        {
            InOutMtx.lock();
            std::string cmd;
            std::cout << "COMMAND: ";
            std::getline(std::cin, cmd); // 读取用户输入命令

            if (cmd.empty()) // 如果输入为空，继续循环
            {
                InOutMtx.unlock();
                continue;
            }

            std::cout << "FTP->" << cmd << std::endl;
            InOutMtx.unlock();

            int nReplyCode;
            if (cmd == "list")
            {
                List_Command();
            }
            else if (cmd == "stor")
            {
                Stor_Command();
            }
            else if (cmd == "retr")
            {
                Retr_Command();
            }
            else if (cmd == "pwd")
            {
                Pwd_Command();
            }
            else if (cmd == "cwd")
            {
                Cwd_Command();
            }
            else if (cmd == "quit")
            {
                Quit_Command();
            }
            else if (cmd == "help")
            {
                Help_Command();
            }
            else
            {
                std::cout << "Invalid command!" << std::endl;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
}

void connect_to_server()
{
    char Command[MAX_SIZE];
    int nReplyCode;
    std::string ReplyMsg; // 使用 std::string 代替 char[] 数组

    // 创建控制连接socket
    SocketControl = socket(AF_INET, SOCK_STREAM, 0);
    if (SocketControl == INVALID_SOCKET)
    {
        std::cout << "Create TCP Control socket error!" << std::endl;
        return;
    }

    // 定义FTP服务器控制连接地址和端口号
    sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(21);
    server_addr.sin_addr.S_un.S_addr = inet_addr(server_ip);

    // 向FTP服务器发送控制连接请求
    std::cout << "FTP control connect..." << std::endl;
    if (connect(SocketControl, (sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        std::cout << "Connection failed!" << std::endl;
        closesocket(SocketControl);
        return;
    }

    std::thread recv_thread(RecvReplyThread);
    recv_thread.detach(); // 开启一个接收响应的线程

    // 获取控制连接上的应答消息
    if (GetReplyFromQueue(ReplyMsg))
    {
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);
        if (nReplyCode == 220)
        {
            std::cout << ReplyMsg << std::endl;
        }
        else
        {
            std::cout << "Unexpected response from server!" << std::endl;
            closesocket(SocketControl);
            return;
        }
    }

    // 向服务器发送USER命令
    std::cout << "FTP->USER:";
    memset(CmdBuf, 0, MAX_SIZE);
    std::cin.getline(CmdBuf, MAX_SIZE); // 输入用户名并保存

    memset(Command, 0, MAX_SIZE);
    memcpy(Command, "USER ", strlen("USER "));
    memcpy(Command + strlen("USER "), CmdBuf, strlen(CmdBuf));
    memcpy(Command + strlen("USER ") + strlen(CmdBuf), "\r\n", 2);
    if (!SendCommand(Command))
        return;

    // 获得USER命令的应答信息
    if (GetReplyFromQueue(ReplyMsg))
    {
        sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);
        if (nReplyCode == 230)
            std::cout << ReplyMsg << std::endl;
        else if (nReplyCode == 331)
        {
            // 向FTP服务器发送PASS命令
            std::cout << "FTP->PASS:";
            memset(CmdBuf, 0, MAX_SIZE);
            std::cin.getline(CmdBuf, MAX_SIZE); // 输入密码并保存

            memset(Command, 0, MAX_SIZE);
            memcpy(Command, "PASS ", strlen("PASS "));
            memcpy(Command + strlen("PASS "), CmdBuf, strlen(CmdBuf));
            memcpy(Command + strlen("PASS ") + strlen(CmdBuf), "\r\n", 2);

            if (!SendCommand(Command))
                return;

            if (GetReplyFromQueue(ReplyMsg))
            {
                sscanf(ReplyMsg.c_str(), "%d", &nReplyCode);
                if (nReplyCode == 230)
                    std::cout << ReplyMsg << std::endl;
                else
                {
                    std::cout << "PASS command reply error" << std::endl;
                    closesocket(SocketControl);
                    return;
                }
            }
        }
        else
        {
            std::cout << "USER command reply error" << std::endl;
            closesocket(SocketControl);
            return;
        }
    }

    // 启动命令输入线程
    std::thread cmd_thread(Command_Thread);
    cmd_thread.join();
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cout << "Usage: ./test <server_ip>" << std::endl;
        return 1;
    }
    // 直接在 main 中给 server_ip 赋值
    strcpy(server_ip, argv[1]);

    WSADATA WSAData;
    if (WSAStartup(MAKEWORD(2, 2), &WSAData) != 0)
    {
        std::cout << "WSAStartup error!" << std::endl;
        return 0;
    }

    connect_to_server();
    return 0;
}
