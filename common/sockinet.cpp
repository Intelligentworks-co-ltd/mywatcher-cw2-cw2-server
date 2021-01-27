
#include "sockinet.h"

#ifdef WIN32

#pragma comment(lib, "wsock32.lib")

void initialize_sockinet()
{
	WORD wVersionRequested;
	int  nErrorStatus;
	WSADATA wsaData;
	wVersionRequested = MAKEWORD(1,1);
	nErrorStatus = WSAStartup(wVersionRequested, &wsaData);
	atexit((void (*)(void))(WSACleanup));
}

#else

void initialize_sockinet()
{
}

#endif
