# FTP 客户端程序 :computer:

这是一个用 C++ 编写的简单 FTP 客户端程序，支持与 FTP 服务器进行通信，执行常见的 FTP 操作命令，如 `RETR`（下载文件）、`CWD`（切换目录）、`QUIT`（退出）、`LIST`（列出文件）等。程序通过多线程实现用户命令交互和文件传输。

## 功能 :gear:

1. **连接到 FTP 服务器**
    用户提供 FTP 服务器的 `IP` 地址，程序将尝试连接到该服务器。
2. **登录到 FTP 服务器**
    用户输入用户名和密码，程序会向服务器发送 `USER` 和 `PASS` 命令进行身份验证。
3. **执行常见的 FTP 命令**
   - `list`：列出当前工作目录下的文件和文件夹。
   - `stor `：上传文件到服务器。
   - `retr `：从服务器下载文件到本地。
   - `pwd`：显示当前工作目录。
   - `cwd `：更改服务器的当前工作目录。
   - `quit`：退出 FTP 客户端并关闭与服务器的连接。
   - `help`：显示所有支持的命令及其使用说明。
4. **多线程处理**
    程序使用多线程来处理命令输入、响应接收和文件下载等任务，以确保客户端的响应速度。

## 编译与运行 :package:

### 编译环境 :wrench:

- 操作系统：Windows
- 编译器：Visual Studio 或其他支持 C++ 的编译器
- 使用 `Winsock` 库进行网络通信

### 编译步骤 :pencil2:

1. 确保安装了 Visual Studio 或其他支持 C++ 的编译器。

2. 下载或克隆项目文件。

3. 在项目文件夹中打开命令行，运行以下命令编译程序：

   ```bash
   g++ -g -o ftp_client ftp_client.cpp -lwsock32 -lws2_32
   ```

4. 编译成功后，运行 `ftp_client.exe`。

### 运行程序 :rocket:

1. 打开终端或命令行，进入编译后的目录。

2. 运行程序并传入 FTP 服务器的 IP 地址：

   ```bash
   ./ftp_client.exe <server_ip>
   ```

3. 根据提示输入用户名和密码，并选择执行 FTP 命令。

## 示例 :sparkles:

每次输入指令前，按**回车**并等待少许时间，就会在屏幕上出现

```
COMMAND:
```

此时用户即可输入相关的指令来执行不同的功能。

### 1. 列出当前目录的文件：

```bash
COMMAND: list
```

### 2. 上传文件：

```bash
COMMAND: stor
FTP->stor
Enter the local file path to upload: test.jpg
Enter the remote file path on the FTP server: rsvtest.jpg
Data connection established to 127.0.0.1:59746
150 正在打开二进制模式数据连接为 rsvtest.jpg.
Starting file upload...
Uploaded 1024 of 517727 bytes (0.197788%)
...
226 传送完毕 (0.000 KB/s).
```

### 3. 下载文件：

```bash
COMMAND: retr
FTP->retr
Enter the filename to retrieve: test.txt
Data connection established to 127.0.0.1:59760
150 正在打开二进制模式数据连接为 test.txt (1470 比特).
Do you want to save the file? (y/n): y
Enter the full save path (e.g., C:\path\to\file.txt): rsctest.txt
Successfully Save
226 传送完毕 (0.000 KB/s).
```

### 4. 更改目录：

```bash
COMMAND: cwd
FTP->cwd
Enter the target directory path to change to: /picture/background
250 成功切换目录
```

### 5. 退出程序：

```bash
Command: quit
```

## 注意事项 :warning:

- 本程序使用 `Winsock` 库来实现 `TCP/IP` 网络通信，确保你已经正确设置了 `Windows` 网络环境。
- 请确保你能连接到 `FTP` 服务器并知道用户名和密码。
- 文件传输（如下载和上传）是通过数据连接进行的，程序会自动处理连接建立和数据传输。



## 主要内容 :memo:

### 1. 每个需要数据传输的指令创建一个数据线程

- `Download_Data_Thread` 负责从 FTP 服务器接收数据并根据用户需求将其保存到文件或者直接输出到控制台。这个函数根据参数 `saveToFile` 的值来决定是否将接收到的数据保存到文件中。

  1. **保存文件**：如果 `saveToFile` 为 `true`，则程序会打开指定路径的文件，并将接收到的数据写入该文件。当数据接收完成后，会关闭数据连接并打印成功信息。
  2. **输出数据**：如果 `saveToFile` 为 `false`，则数据会直接输出到控制台，而不会保存到文件。
  3. **处理接收数据**：无论是保存还是输出，程序都通过 `recv` 从数据连接中读取数据，并使用缓冲区处理。接收完数据后，程序会通知控制台输出结果并关闭连接。

  示例代码：

  ```
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
                  outFile.write(Buffer, nRecv); // 写入文件
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
              sscanf(ReplyMsg.c_str(), "%d", &nReplyCode); // 解析响应码
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
              std::cout << Buffer; // 输出到控制台
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
  ```

- `Upload_Data_Thread` 用于将本地文件上传至 FTP 服务器。此线程负责读取文件内容并通过数据连接发送至服务器，显示上传进度，并在上传完成后处理服务器的响应。

  - 

  1. **文件读取**：程序使用 `fread` 从本地文件中读取数据并通过 `send` 函数将其发送到 FTP 服务器。每次读取一定量的字节并发送。
  2. **上传进度显示**：在文件上传的过程中，线程会计算并显示当前上传进度，确保用户能够实时看到上传情况。
  3. **错误处理**：如果在发送数据时发生错误（如连接断开等），线程会关闭数据连接并报告错误。
  4. **上传完成**：文件上传完成后，线程会关闭文件流和数据连接，等待并打印服务器的响应。

  示例代码：

  ```
  void Upload_Data_Thread(SOCKET SocketData, FILE *file)
  {
      char buffer[1024];
      int bytesRead;
      int totalBytesSent = 0;   // 记录总共发送的字节数
      fseek(file, 0, SEEK_END);  // 获取文件大小
      long fileSize = ftell(file);
      fseek(file, 0, SEEK_SET);  // 重置文件指针到文件开始位置
  
      // 显示开始上传信息
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
  ```

- 这两个线程函数使得 FTP 客户端在进行数据传输时能够同时执行多项任务，不会阻塞用户操作，同时还能够显示实时的进度信息，提高了程序的交互性和效率。

### 2. 使用InOutMtx使控制台有序输出

在程序中，为了确保多线程操作时控制台的输出顺序和避免混乱，使用了一个互斥锁（`InOutMtx`）来保护所有的控制台输出部分。这样可以确保每次只有一个线程访问标准输入输出流，从而避免多个线程同时打印信息导致的输出冲突。

例如，程序在接收来自 FTP 服务器的响应信息时，使用 `InOutMtx.lock()` 和 `InOutMtx.unlock()` 来保证输出信息的顺序性和一致性。每当需要输出命令执行结果时，都会首先获得锁，执行输出操作，最后释放锁。

示例代码：

```
InOutMtx.lock();
std::cout << "FTP->" << cmd << std::endl;
InOutMtx.unlock();
```

这样做有效避免了多线程情况下，多个线程同时输出到控制台时可能出现的信息错乱，确保了命令和响应的打印按顺序执行。

### 3. 使用一个线程不断地接收服务器返回的控制消息并将其维护在一个队列中

在本程序中，使用了一个独立线程来不断地接收来自 FTP 服务器的控制消息，并将这些消息存储到一个线程安全的队列中。这样，主线程可以从队列中读取并处理这些消息，确保多线程环境下消息的正确传递与处理。

具体实现是通过一个接收线程 `RecvReplyThread()` 来持续接收数据并按行存储到队列中，同时使用条件变量 `cv` 来同步等待队列中的数据。

```
std::queue<std::string> msgQueue; // 用于存储接收到的消息
std::mutex mtx;                   // 用于保护消息队列的互斥量
std::condition_variable cv;       // 用于线程同步
```

接收线程通过 `recv()` 函数从服务器接收数据，并将数据按 `\r\n` 分隔成多个响应存入队列。每当有新消息加入队列时，接收线程会通知其他线程进行处理。

示例代码：

```
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
```

在 `RecvReplyThread()` 函数中，首先使用 `recv()` 函数接收来自服务器的消息，如果接收成功，就使用 `strtok()` 函数将消息按照 `\r\n` 进行分割，并将每个分割出的响应加入到消息队列 `msgQueue` 中。在每次消息被成功加入队列后，调用 `cv.notify_one()` 来通知等待的线程，表示队列中有新消息可以处理。

另一方面，主线程或其他处理线程可以通过 `GetReplyFromQueue()` 函数从队列中读取响应消息。

示例代码：

```
bool GetReplyFromQueue(std::string &replyMsg)
{
    std::unique_lock<std::mutex> lock(mtx);
    cv.wait(lock, []() { return !msgQueue.empty(); }); // 等待直到队列中有数据
    replyMsg = msgQueue.front();
    msgQueue.pop();
    return true;
}
```

在 `GetReplyFromQueue()` 函数中，使用条件变量 `cv.wait()` 来确保线程等待直到队列中有新的消息可以处理。当队列非空时，线程将从队列中获取一条响应并返回给调用者。

通过这种设计，程序能够高效地管理多线程环境下的消息传递，避免了消息丢失和竞态条件，同时确保消息的顺序性和一致性。





