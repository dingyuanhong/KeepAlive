#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>

#include <poll.h>
#include <errno.h>
#include "log.h"
#include "libkeepalive.h"

#ifdef _WIN32
#include <windows.h>
#define tp_socket SOCKET
#else
#define tp_socket int
#endif
#include "os.h"

int sock_write(tp_socket fd, char* buf, int len)
{
    int err_num, res;
    int disconnect = 0;

    while(!disconnect){
        /* Use send with a MSG_NOSIGNAL to ignore a SIGPIPE signal which
         * would cause the process exit. */
        res = send(fd, buf, len, MSG_NOSIGNAL | MSG_DONTWAIT);
        if( -1 == res ){
            err_num = errno;
            switch(err_num){
                case ECONNRESET:
                case EBADF:
                case EPIPE:
                case ENOTSOCK:
                    disconnect = 1;
					VLOGE("send error:%d errno:%d",res,err_num);
                    break;
                //case EWOULDBLOCK:
                case EAGAIN:
                    sleep(1);
                    break;
                case EINTR:
                    break;
				case 0:
					sleep(1);
					break;
                default:
					disconnect = 1;
					VLOGE("send error default:%d errno:%d",res,err_num);
                    break;
            }
        }else if( res > 0 ){
            if( res < len ){
                /* Incomplete information. */
                buf += res;
                len -= res;
            }else if( res == len ){
                /* Message has sended successfully. */
                break;
            }
        }
    }
    return disconnect;
}

void* write_handle(void* fd)
{
	tp_socket connection_fd;
    int len;
    char buffer[256];
    char* end_flag = "\r\n";

    connection_fd = *(tp_socket*)fd;
    len = snprintf(buffer, 256, "buffer data ends with a end flag%s", end_flag);
    buffer[len] = 0;

    while(0 == sock_write(connection_fd, buffer, len)){
        sleep(1);
    }
}

int main(int argc, char* argv[])
{
    struct sockaddr_in address;
    tp_socket socket_fd, connection_fd;
    socklen_t address_length;

	char * ip = "0.0.0.0";
	int port = 5555;
	if (argc >= 2) {
		port = atoi(argv[1]);
	}

	VLOGD("set port:%d", port);

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	address.sin_addr.s_addr = inet_addr(ip);

	setKeepAliveParam(1,3,2,3);

    socket_fd = keepaliveSocket(AF_INET,SOCK_STREAM,0);
    if(socket_fd < 0)     {
		 VLOGE("socket error: errno:%d",errno);
         return -1;
     }

	 int ret = bind(socket_fd, (const struct sockaddr *) &address, (int)sizeof(struct sockaddr_in));
     if(ret != 0)
     {
         VLOGE("bind error:%d errno:%d",ret,errno);
         return -1;
     }
	 ret = listen(socket_fd, 0);
     if(ret != 0)
     {
         VLOGE("listen error:%d errno:%d",ret,errno);
         return -1;
     }
	 cloexec(socket_fd);
	 keepalive(socket_fd,1);
     while((connection_fd = accept(socket_fd, (struct sockaddr *) &address,&address_length)) > -1)
     {
        pthread_t w_thread;
        char buffer_pool[512];
		char *buffer = buffer_pool;
		int buffer_totle = 512;
        int nbytes;
        int cnt;
        int err_num;
        int disconnect=0;

		nonBlocking(connection_fd);
		keepalive(connection_fd,1);
		cloexec(connection_fd);
		setSocket(connection_fd);

		VLOGD("accept connection:%d",connection_fd);
        int ret = pthread_create(&w_thread, NULL, write_handle, &connection_fd);
		if (ret == 0)
		{
			while(1){
	            // if result > 0, this means that there is either data available on the
	            // socket, or the socket has been closed
	            cnt = recv(connection_fd, buffer, buffer_totle, MSG_PEEK);
	            if( 0 == cnt ){
	                // if recv returns zero, that means the connection has been closed.
					VLOGD("recv closed:%d",connection_fd);
	                disconnect = 1;
	                break;
	            }else if( -1 == cnt ){
	                err_num = errno;
	                switch(err_num){
	                    case ECONNRESET:
	                    case EBADF:
	                    case EPIPE:
	                    case ENOTSOCK:
	                        disconnect = 1;
							VLOGE("recv error:%d errno:%d",cnt,err_num);
	                        break;
						case EAGAIN:
							sleep(1);
							break;
						case 0:
							sleep(1);
							break;
	                    default:
							disconnect = 1;
							VLOGE("recv error default:%d errno:%d",cnt,err_num);
	                        break;
	                }
	                if( disconnect ){
	                    break;
	                }
	            }else if( cnt > 0 ){
	                /* discard everything received from client.*/
	                while((nbytes = recv(connection_fd, buffer, buffer_totle-1, 0)) > 0){
	                    buffer[nbytes] = 0;
						VLOGD("recv success:%d",nbytes);
	                }
	            }
	        }
	        close(connection_fd);
	        pthread_cancel(w_thread);
		}else
		{
			close(connection_fd);
			VLOGE("pthread_create error：%d",ret);
		}
    }

    close(socket_fd);
    return 0;
}
