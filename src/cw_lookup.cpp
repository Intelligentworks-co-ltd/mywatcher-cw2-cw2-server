#include "common/sockinet.h"
#include "cw_lookup.h"

#include <unistd.h>

void on_bad_alloc(std::bad_alloc const &e, char const *file, int line);

void set_socket_recv_timeout(socket_t sock, int sec)
{
	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv, sizeof(tv));
}

void set_socket_send_timeout(socket_t sock, int sec)
{
	struct timeval tv;
	tv.tv_sec = sec;
	tv.tv_usec = 0;
	setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv, sizeof(tv));
}


namespace ContentsWatcher {

	NameLookupThread::NameLookupThread()
	{
		sock = INVALID_SOCKET;
	}

	void NameLookupThread::run()
	{
		unsigned char buf[10000];
		struct sockaddr_in sa;
		socklen_t fromsize;
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET) {
			return;
		}

		set_socket_recv_timeout(sock, 1);
		
		sa.sin_family = AF_INET;
		sa.sin_port = htons(33699);
		sa.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(sock, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
			return;
		}
		while (!interrupted()) {
			try {
				fromsize = sizeof(sa);
				int n = recvfrom(sock, (char *)buf, sizeof(buf), 0, (struct sockaddr *)&sa, &fromsize);
				if (n < 0) {
					continue;
				}

			} catch (std::bad_alloc const &e) {
				on_bad_alloc(e, __FILE__, __LINE__);
			}
		}

		closesocket(sock);
	}

	void NameLookupThread::stop()
	{
		::Thread::stop();
	}

}
