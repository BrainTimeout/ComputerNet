# socket编程

socket是“open—write/read—close”模式的一种实现，表示建立的一个通讯关系，通过socket函数，可以对这个socket进行操作。

使用函数socket()创建socket,返回值是一个整型，表示了这个socket的id，是对这个socket的一种标识。

**sockid＝socket（af，type，protocol）** 

- af(Address Family)—网络地址类型，常用的有AF_INET、AF_INET6、AF_LOCAL、AF_ROUTE

  其中AF_INET代表使用ipv4地址；

- type—传输层通信协议类型，常用的socket类型有，SOCK_STREAM、SOCK_DGRAM、SOCK_RAW、 SOCK_PACKET、SOCK_SEQPACKET等,

  - SOCK_STREAM：表示面向连接的字节流通信方式

  - SOCK_DGRAM：表示无连接的数据报通信方式；

  - SOCK_RAW：原始数据报，允许直接操作协议头（如IP头和ICMP头），需手动构造整个数据包。用于低级网络通信（例如Ping命令）

- protocol— 网络通信协议，常用的协议有，IPPROTO_TCP、IPPTOTO_UDP、IPPROTO_SCTP、 IPPROTO_TIPC等， 也可使用IPPROTO_IP =0 ，根据type定； 

- 返回值sockid是一个整数(句柄)，即socket号；双方都知道五元组信息，双方就可以发送和接收数据了。

- 一般情况下：客户进程首先知道五元组信息；在TCP协议下，只有三次握手后，服务器进程 才知道五元组信息；在UDP协议下，只有服务器进程收到客户进程服务请求后，才知道五元 组信息。 

- 用法： 

  ```
  SOCKET sockid = socket(AF_INET,SOCK_DGRAM,IPPROTO_IP); 
  if(sockid==INVALID_SOCKET) { //错误处理 }
  ```


## 运行环境

```
#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")
```

在编译时，使用如下方法：

```
g++ -g -o udp_client1 udp_client1.cpp -lwsock32 -lws2_32
```

## udp

服务器socket建立后，使用bind()将该socket与固定的IP和port绑定。

客户端socket建立后，可使用bind()将该socket与固定的IP和port绑定，也可不与自身的IP和port绑定，此时该socket使用IP和port为操作系统提供。

客户端和服务端都可以通过

```
struct sockaddr_in addr;
char ip[INET_ADDRSTRLEN];
unsigned short port = 9543;

memset(&addr, 0, sizeof(addr));
client_addr.sin_family = AF_INET;
client_addr.sin_port = htons(port);
inet_pton(AF_INET, ip, &addr.sin_addr);
```

创建一个地址和端口的sockaddr_in结构体



采用UDP协议进行数据发送调用必须明确指定接收方socket地址: 

```
sendto（sockid，buf，buflen，flags，destadd，addrlen） 
```

- 返回值为成功发送数据字节数 

- sockid为自己的，destadd和addrlen为创建好的包含接收方信息的sockaddr_in结构体及其长度



采用UDP协议数据接收 

```
recvfrom（sockid，buf，buflen，flags，souradd，addrlen）
```

- 返回值为成功接收数据字节数 

- sockid为自己的，destadd和addrlen为创建好的关于接收方信息的sockaddr_in结构体及其长度，这个结构体信息是在recvfrom函数中被填写



  关闭套接字并清理 Winsock

```
closesocket(client_sockid);
WSACleanup();
```



## TCP

服务器socket(listen)建立后，使用bind()将该socket与固定的IP和port绑定。

客户端socket建立后，可使用bind()将该socket与固定的IP和port绑定，也可不与自身的IP和port绑定，此时该socket使用IP和port为操作系统提供。

### 在服务器中

如果有客户端连接请求，则把请求放入到等待队列中排队等待处理， 其调用格式为：

`listen（sockid，quelen）`


- Sockid：本地socket号，服务器进程在此socket地址上接收连接 请求。
- quelen：服务端连接请求队列长度；此参数限制连接请求的排队 长度，通常允许的连接请求排队长度最大值。 
- 用法： 

```
int nResult=listen(sockid,5)//最多5个连接
if(nResult==SOCKET_ERROR) { //错误处理 }
```



服务器进程处理客户进程的连接请求:

`accept( )`

当等待队列没有连接请求时，服务进程处于侦听状态，否则，响应并处理连接请求。其调用格式如下：

`new-sockid = accept（sockid，clientaddr，addrlen）`

- Sockid：本地用于监听socket号。包含了server的IP和port，这个信息被用于与client的信息组合形成新的socket，而该socket则一直不变。

- clientaddr，指向客户端socket地址结构的指针，初始值为空，当accept调用 返回后，客户进程的socket地址被填入该地址结构中.

- addrlen，初值为0，当accept调用返回后存放客户socket地址长度。

- new-sockid：返回值是一个新创建的socket，该socket维护的是服务器与接收到的该客户端的通讯，是包含双方信息的五元组

- 用法：

  ```c++
   sockaddr_in clientaddr; SOCKET newsockid =  accept(sockid,(sockaddr*)&clientaddr,sizeof(sockaddr)) if(newsockid==INVALID_SOCKET) { //错误处理 }
  ```



### 在客户端中

采用TCP协议通信的客户进程通过调用connect( ) 主动请 求与服务器建立连接，其调用格式为：

`connect（sockid，Seraddr，Seradd-len ）` 

- Sockid：本地socket号。 

- Seraddr：是一个指向服务器的socket地址结构（包括IP地址、端口 等信息）的指针。 

- Seraddrlen：服务器socket地址结构长度。

- 用法：

  ```
  int nResult=connect(sockid,(sockaddr*)&Seraddr,sizeof(sockaddr));
  if(nResult==SOCKET_ERROR){//错误处理}
  ```

- connect函数调用结束后，该socket绑定了服务器的信息，形成了包含双方信息的五元组

### 发送和接收消息

服务器和客户端都可以发送和接受消息，客户端使用自己的socket，服务器使用新创建的socket

发送数据可以不必指定接收方socket地址，因为sockid中已包含通信的5元组: 

`send（sockid，buf，buflen，flags）` 

- 返回值为成功发送数据字节数。



接收数据调用与发送数据调用是一一对应的：

`recv（sockid，buf，buflen，flags`

- 返回值为成功接收数据字符数。



### 关闭套接字

释放所占有的资源，服务器端和客户端使用。 

`int closesocket( SOCKET socketid )` 

- 用法： 

  ```
  int nResult=closesocket(socketid); if(nResult==SOCKET_ERROR) { //错误处理 }
  ```

