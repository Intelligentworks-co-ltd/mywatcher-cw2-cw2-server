
#ifndef __SOCKINET_H
#define __SOCKINET_H


#ifdef WIN32
	#include <winsock2.h>
	typedef SOCKET socket_t;
	typedef int socklen_t;
#else
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	typedef int socket_t;
	#define INVALID_SOCKET (-1)
	#define closesocket(S) close(S)
#endif



void initialize_sockinet();

void set_socket_recv_timeout(socket_t sock, int sec);
void set_socket_send_timeout(socket_t sock, int sec);


#endif
