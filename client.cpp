#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>

#include <errno.h>
#include "log.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Mstcpip.h>
#include <windows.h>

#define tp_socket SOCKET
#define close closesocket
#define sleep Sleep

#pragma comment(lib,"ws2_32.lib")

void InitNewwork()
{
	WSADATA  Ws;
	//Init Windows Socket
	if (WSAStartup(MAKEWORD(2, 2), &Ws) != 0)
	{
		//VLOGD("初始化失败");
		return;
	}
}

#else

#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#define tp_socket int

#endif

#include "os.h"

int main(int argc, char* argv[])
{
#ifdef _WIN32
	InitNewwork();
#endif
	if (argc < 3) {
		VLOGE("input：ip port");
		return -1;
	}
	char * ip = argv[1];
	int port = atoi(argv[2]);

    struct sockaddr_in address;
    tp_socket  socket_fd;
    int err_num;

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = inet_addr(ip);

	VLOGD("connect test begin:");
    while(1){
        socket_fd = socket(AF_INET,SOCK_STREAM,0);
		VASSERT(socket_fd != -1);
        if(socket_fd < 0)
        {
			 VLOGE("socket error: errno:%d",errno);
             sleep(1);
             continue;
        }
		int ret = connect(socket_fd, (const struct sockaddr *) &address, (int)sizeof(struct sockaddr_in));
        if(ret != 0)
        {
			VLOGE("connect error:%d errno:%d",ret,errno);
            close(socket_fd);
            socket_fd = -1;
            sleep(1);
            continue;
        }

		nonBlocking(socket_fd);
		keepalive(socket_fd,1);

		VLOGD("connect server success.");
		//VLOGD("连接服务器成功");
		int disconnect = 0;
#if 1
        while(1){
                // if result > 0, this means that there is either data available on the
                // socket, or the socket has been closed
				char buffer_pool[512];
				char *buffer = buffer_pool;
				int buffer_totle = 512;
                int cnt, nbytes;
                cnt = recv(socket_fd, buffer, buffer_totle-1, MSG_PEEK);
                if( -1 == cnt){
                    err_num = errno;
                    switch(err_num){
                    case ECONNRESET:
                    case EBADF:
                    case EPIPE:
                    case ENOTSOCK:
                        VLOGD("recv closed:%d errno:%d",cnt,err_num);
						disconnect = 1;
                        break;
					case EAGAIN:
		                sleep(1);
		                break;
					case 0:
						sleep(1);
						break;
                    default:
						VLOGD("recv closed default:%d errno:%d",cnt,err_num);
						sleep(1);
						disconnect = 1;
                        break;
                    }
                    if(disconnect){
                        break;
                    }
                }
                else if( 0 == cnt){
         			// if recv returns zero, that means the connection has been closed:
         			disconnect = 1;
                    break;
                }
                else if( cnt > 0 ){
                    while((nbytes = recv(socket_fd, buffer, buffer_totle-1, 0)) > 0){
                        buffer[nbytes] = 0;
                        //printf("buffer:%s\n", buffer);
                        VLOGD("recv success:%d",nbytes);
                    }
                }
        }
#endif
        close(socket_fd);
        socket_fd = -1;
		VLOGD("connect closed");
        /* here sleep 5 seconds and re-connect. */
        sleep(5);
    }
	VLOGD("connect test over");
    return 0;
}
