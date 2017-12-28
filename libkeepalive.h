#ifndef LIBKEEPALIVE_H
#define LIBKEEPALIVE_H

int keepaliveSocket(int domain, int type, int protocol);

int setKeepAliveParam(int keepalive,int idle,int intvl,int count);

#endif
