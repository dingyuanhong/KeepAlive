#ifndef OS_H
#define OS_H

static inline int getArgumentInt(int pos,int argc, char* argv[],const char * name,int def)
{
	for(int i = pos ; i < argc;i+=2)
	{
		bool foundcmd = false;
		char * value = argv[i];
		if(strcmp(name,value) == 0)
		{
			foundcmd = true;
		}
		if(i + 1 >= argc) continue;
		if(foundcmd)
		{
			value = argv[i+1];
			int ret = atoi(value);
			return ret;
		}
	}
	return def;
}

static inline char* getArgumentPChar(int pos,int argc, char* argv[],const char * name,char* def)
{
	for(int i = pos ; i < argc;i+=2)
	{
		bool foundcmd = false;
		char * value = argv[i];
		if(strcmp(name,value) == 0)
		{
			foundcmd = true;
		}
		if(i + 1 >= argc) continue;
		if(foundcmd)
		{
			return argv[i+1];
		}
	}
	return def;
}

static inline void nonBlocking(tp_socket socket)
{
#ifdef _WIN32
	unsigned long flags = 1;
	ioctlsocket(socket, FIONBIO, &flags);
#else
	int flags = fcntl(socket, F_GETFL, 0);
	int ret = fcntl(socket, F_SETFL, flags | O_NONBLOCK);
	VLOGD("nonBlocking set:%d",ret);
#endif
}

static inline void Reuse(tp_socket socket,int reuse)
{
#ifdef _WIN32
	unsigned long flags = reuse;
	ioctlsocket(socket, SO_REUSEADDR, &flags);
#else
	setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (const void *)&reuse, sizeof(reuse));
#endif
}

static inline void cloexec(tp_socket socket)
{
#ifndef _WIN32
	int flags = fcntl(socket, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(socket, F_SETFD, flags);
#endif
}

static inline void keepalive(tp_socket socket,int keep)
{
#ifdef _WIN32
	VLOGD("win32 setsockopt set keepalive ...");
	BOOL bSet= (keep != 0);
	int  ret = setsockopt(socket,SOL_SOCKET,SO_KEEPALIVE,(const char*)&bSet,sizeof(BOOL));
	if(ret == 0){
		tcp_keepalive live,liveout;
		live.keepaliveinterval=2000; //每次检测的间隔 （单位毫秒）
		live.keepalivetime=3000;  //第一次开始发送的时间（单位毫秒）
		live.onoff=1;
		DWORD dw = 0;
		//此处显示了在ACE下获取套接字的方法，即句柄的(SOCKET)化就是句柄
		ret = WSAIoctl(socket, SIO_KEEPALIVE_VALS, &live, sizeof(live), &liveout, sizeof(liveout), &dw, NULL, NULL);
		if(ret != 0)
		{
			VLOGE("win32 WSAIoctl error:WSAGetLastError:%d %d",ret, WSAGetLastError());
		}
		VLOGD("win32 setsockopt set keepalive over ");
	}else{
		VLOGE("win32 setsockopt error: errno:%d",errno);
	}
#else
	VLOGD("setsockopt set keepalive ...");
	int keepAlive = keep;//设定KeepAlive
	int keepIdle = 3;//开始首次KeepAlive探测前的TCP空闭时间
	int keepInterval = 2;//两次KeepAlive探测间的时间间隔
	int keepCount = 3;//判定断开前的KeepAlive探测次数
	VLOGD("setsockopt param: keepalive:%d idle:%d interval:%d count:%d", keepAlive, keepIdle, keepInterval, keepCount);
	if (setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive)) != 0)
	{
		VLOGE("setsockopt SO_KEEPALIVE error.(%d)", errno);
	}
	#ifdef __MAC__
	#else
	if (setsockopt(socket, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, sizeof(keepIdle)) != 0)
	{
		VLOGE("setsockopt TCP_KEEPIDLE error.(%d)", errno);
	}
	if (setsockopt(socket, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, sizeof(keepInterval)) != 0)
	{
		VLOGE("setsockopt TCP_KEEPINTVL error.(%d)", errno);
	}
	if (setsockopt(socket, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, sizeof(keepCount)) != 0)
	{
		VLOGE("setsockopt TCP_KEEPCNT error.(%d)", errno);
	}

	socklen_t len_keepalive = sizeof(keepAlive);
	socklen_t len_keepIdle = sizeof(keepIdle);
	socklen_t len_keepInterval = sizeof(keepInterval);
	socklen_t len_keepCount = sizeof(keepCount);
	int ret = getsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, &len_keepalive);
	if (ret != 0) VLOGE("getsockopt SO_KEEPALIVE error:%d",errno);
	ret = getsockopt(socket, SOL_TCP, TCP_KEEPIDLE, (void *)&keepIdle, &len_keepIdle);
	if (ret != 0) VLOGE("getsockopt TCP_KEEPIDLE error:%d", errno);
	ret = getsockopt(socket, SOL_TCP, TCP_KEEPINTVL, (void *)&keepInterval, &len_keepInterval);
	if (ret != 0) VLOGE("getsockopt TCP_KEEPINTVL error:%d", errno);
	ret = getsockopt(socket, SOL_TCP, TCP_KEEPCNT, (void *)&keepCount, &len_keepCount);
	if (ret != 0) VLOGE("getsockopt TCP_KEEPCNT error:%d", errno);
	VLOGD("getsockopt param: keepalive:%d idle:%d interval:%d count:%d", keepAlive, keepIdle, keepInterval, keepCount);
	VLOGD("setsockopt set keepalive over");
	#endif
#endif
}

static inline void setSocket(tp_socket socket)
{
	int nNetTimeout = 1000; //1秒
	int nSendBufLen = 1 * 1024; //设置为32K
	int nRecvBufLen = 1 * 1024; //设置为32K
	int nZero = 0;
	struct linger m_sLinger;

	setsockopt( socket, SOL_SOCKET, SO_SNDTIMEO, (const char * )&nNetTimeout, sizeof( int ) );
	setsockopt( socket, SOL_SOCKET, SO_RCVTIMEO, (const char * )&nNetTimeout, sizeof( int ) );
	//设置缓冲区
	setsockopt( socket, SOL_SOCKET, SO_SNDBUF, ( const char* )&nSendBufLen, sizeof( int ) );
	setsockopt( socket, SOL_SOCKET, SO_RCVBUF, ( const char* )&nRecvBufLen, sizeof( int ) );
	//不执行将socket缓冲区的内容拷贝到系统缓冲区
	setsockopt( socket, SOL_SOCKET, SO_SNDBUF, (const char * )&nZero, sizeof( int ) );
	setsockopt( socket, SOL_SOCKET, SO_RCVBUF, (const char * )&nZero, sizeof( int ) );

	//设置关闭时清楚数据时限
	m_sLinger.l_onoff = 1; //在调用close(socket)时还有数据未发送完，允许等待
	// 若m_sLinger.l_onoff=0;则调用closesocket()后强制关闭
	m_sLinger.l_linger = 5; //设置等待时间为5秒
	setsockopt( socket, SOL_SOCKET, SO_LINGER, (const char*)&m_sLinger, sizeof(struct linger));
}

#endif
