
// Windows用・簡易syslog

#ifdef WIN32

// localhostのsyslogサーバにメッセージを投げる

#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <sstream>


#include "debuglog.h"

#pragma comment(lib, "wsock32.lib")

#pragma warning(disable:4996)

static char const *syslog_ident = 0;
static int syslog_facility = LOG_LOCAL0;

void send_syslog_debug_message(int priority, char const *textptr, size_t textlen)
{
//#ifdef _DEBUG

	SOCKET sock;		/* ソケット（Soket Descriptor） */

	/* sockにソケットを作成します */
	sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		fprintf(stderr,"ソケット作成失敗\n");
	} else {

		int port = 514;

		unsigned long serveraddr;		/* サーバのIPアドレス */

		/* servernameにドットで区切った10進数のIPアドレスが入っている場合", "serveraddrに32bit整数のIPアドレスが返ります */
		//serveraddr = inet_addr("192.168.100.13");
		serveraddr = inet_addr("127.0.0.1");

		struct  sockaddr_in	 serversockaddr;		/* サーバのアドレス */

		/* サーバのアドレスの構造体にサーバのIPアドレスとポート番号を設定します */
		serversockaddr.sin_family	 = AF_INET;				/* インターネットの場合 */
		serversockaddr.sin_addr.s_addr  = serveraddr;				/* サーバのIPアドレス */
		serversockaddr.sin_port	 = htons((unsigned short)port);		/* ポート番号 */
		memset(serversockaddr.sin_zero,(int)0,sizeof(serversockaddr.sin_zero));

		/* サーバを指定して文字列(buf)を送信します */
		/* 送信した文字列は指定したサーバに届きます */

		static char *mon[] = {
			"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
		};
		struct tm tm;
		time_t t = time(0);
		localtime_s(&tm, &t);
		char tmp[100];
		int fac = syslog_facility;
		int sev = priority;
		sprintf(tmp, "<%u>%s %u %02u:%02u:%02u ", fac * 8 + sev, mon[tm.tm_mon], tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
		std::stringstream out;

		char const *ident = syslog_ident;
		if (ident) {
			out.write(ident, (int)strlen(ident));
			out.write(" ", 1);
		}

		out.write(tmp, (int)strlen(tmp));
		out.write(textptr, (int)textlen);
		std::string str = out.str();
		if (sendto(sock, str.c_str(), (int)str.size(), 0,(struct sockaddr *)&serversockaddr,sizeof(serversockaddr)) == SOCKET_ERROR) {
			//fprintf(stderr,"サーバへの送信失敗\n");
		}
	}

//#endif
}

void send_syslog_debug_message(int priority, std::string const &message)
{
	send_syslog_debug_message(priority, message.c_str(), message.size());
}

void send_syslog_debug_message(int priority, std::wstring const &message)
{
	char tmp[10000];
	int n = WideCharToMultiByte(CP_ACP, 0, message.c_str(), (int)message.size(), tmp, sizeof(tmp), 0, 0);
	std::string text(tmp, n);
	send_syslog_debug_message(priority, text);
}

#include <stdarg.h>

void openlog(char const *ident, int option, int facility)
{
	syslog_ident = ident;
	syslog_facility = facility;
}

void syslog(int priority, const char *str, ...)
{
	char tmp[10000];
	va_list ap;
	va_start(ap, str);
	vsprintf(tmp, str, ap);
	va_end(ap);
	send_syslog_debug_message(priority, tmp);
}

void closelog()
{
}


#endif
