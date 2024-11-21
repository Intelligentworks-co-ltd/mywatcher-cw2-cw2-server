#include "common/sockinet.h"

#ifdef WIN32
#else
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#endif

#include "config.h"
#include "TheServer.h"

#include <stdarg.h>

#ifdef WIN32
#include <winsvc.h>

#include <stdio.h>
#include <time.h>

///////////////////////////////////////////////////////////////////////////////
// Windowsサービス

SERVICE_STATUS_HANDLE g_srv_status_handle;
SERVICE_STATUS g_srv_status = {
	SERVICE_WIN32_OWN_PROCESS,
	SERVICE_START_PENDING,
	SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_POWEREVENT,
	NO_ERROR,
	NO_ERROR,
	0,
	0
};


// サービスハンドラ
DWORD TheServer::ServiceHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData)
{
	switch (dwControl) {
	case SERVICE_CONTROL_STOP:
		g_srv_status.dwCurrentState = SERVICE_STOP_PENDING;
		g_srv_status.dwWin32ExitCode = 0;
		g_srv_status.dwCheckPoint = 0;
		g_srv_status.dwWaitHint = 0;
		SetServiceStatus(g_srv_status_handle, &g_srv_status);
		break;
	}
	return NO_ERROR;
}


DWORD WINAPI HandlerEx(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
	TheServer *s = (TheServer *)lpContext;
	return s->ServiceHandler(dwControl, dwEventType, lpEventData);
}

void WINAPI ServiceMain(DWORD ac, char **av)
{
	TheServer *server = get_the_server();
	g_srv_status_handle = RegisterServiceCtrlHandlerEx(WINDOWS_SERVICE_NAME, HandlerEx, server);
	if (!g_srv_status_handle) {
		return;
	}

	server->main(true);
}

#else // WIN32

#include <sys/file.h>
#include <sys/wait.h>

#include <pwd.h>
#include <vector>

static void on_signal(int sig)
{
	TheServer *server = get_the_server();
	if (sig == SIGTERM) {
		signal(SIGTERM, SIG_IGN);
		server->log_printf(0, 0, "caught a SIGTERM");
		server->_sigterm = true;
		return;
	}
	server->log_printf(0, 0, "caught a signal %d", sig);
}

bool personate(char const *username)
{
	std::vector<unsigned char> buffer;
	struct passwd *p = getpwnam(username);
	if (!p) {
		struct passwd pw;
		int n = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (n < (int)sizeof(struct passwd)) {
			return false;
		}
		buffer.resize(n);
		if (getpwnam_r(username, &pw, (char *)&buffer[0], buffer.size(), &p) != 0 || !p) {
			fprintf(stderr, "could not personate to the user '%s'\n", username);
			return false;
		}
	}
	setuid(p->pw_uid);
	setgid(p->pw_gid);
	return true;
}

#endif // WIN32

#include <sys/types.h>

#include "cw_server.h"

int TheServer::main(bool as_service)
{
	set_as_service(as_service);

	initialize_sockinet();

#ifdef WIN32	

	if (_as_service) {
		//サービスステータスを起動中に変更
		g_srv_status.dwCurrentState = SERVICE_START_PENDING;
		g_srv_status.dwCheckPoint = 0;
		g_srv_status.dwWaitHint = 1000;
		SetServiceStatus(g_srv_status_handle, &g_srv_status);
	}

	run();

	if (_as_service) {
		g_srv_status.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(g_srv_status_handle, &g_srv_status);
	}

#else

	if (_as_service) {

		// デーモンになる
		daemon(1, 1);

restart:;

		int pid_fd = open(PID_FILE_PATH, O_RDWR | O_CREAT, 0644);
		if (pid_fd == -1) {
			fprintf(stderr, "could not open pid file (%s)\n", PID_FILE_PATH);
			perror("cws_server");
			return 1;
		}
		if (flock(pid_fd, LOCK_EX | LOCK_NB) != 0) {
			fprintf(stderr, "could not acquire lock. already running ?\n");
			return 1;
		}
//#define PREFORK 1

#ifdef PREFORK
		pid_t child_pid = fork();

		if (child_pid == 0) {

			signal(SIGTERM, on_signal);

//			// 権限を降格
//			if (!personate(DAEMON_USER_NAME)) {
//				return 1;
//			}

			// サーバ実行
			run();

			return 0;
		}

		{
			char tmp[20];
			sprintf(tmp, "%u", child_pid);
			ftruncate(pid_fd, 0);
			write(pid_fd, tmp, strlen(tmp));
		}

		int status = 0;
		waitpid(child_pid, &status, 0);

		close(pid_fd);
		unlink(PID_FILE_PATH);

		if (WIFSIGNALED(status)) {
			if (WTERMSIG(status) == SIGHUP) {
				goto restart;
			}
		}
#else // PREFORK

		signal(SIGTERM, on_signal);

		pid_t pid = getpid();
		{
			char tmp[20];
			sprintf(tmp, "%u", pid);
			ftruncate(pid_fd, 0);
			write(pid_fd, tmp, strlen(tmp));
		}

		run();

		close(pid_fd);
		unlink(PID_FILE_PATH);

		return 0;
#endif // PREFORK
	} else {

		// サーバ実行
		run();

	}

#endif

	return 0;
}





void TheServer::log_print(LogSender const *sender, microsecond_t us, char const *str)
{
	LogItem item;
	if (us == 0) {
		item.time = time(0) * 1000000ULL;
	} else {
		item.time = us;
	}
	if (sender) {
		item.from = sender->ConnectedFrom;
		item.threadnumber = sender->get_thread_number();
	} else {
		item.threadnumber = -1;
	}
	item.text = str;
	make_log_line(&item);

	try {
		AutoLock lock(&_log_data.mutex); // キューにアクセスするためにロックする
		if (_log_data.queue.size() < CW_LOG_QUEUE_LIMIT) {
			_log_data.queue.push_back(item);
			_log_data.event.signal(); // ログスレッドにイベントを投げる
		}
	} catch (std::bad_alloc const &) {
	}
}

void TheServer::log_vprintf(LogSender const *sender, microsecond_t us, char const *str, va_list ap)
{
	char tmp[10000];
#ifdef WIN32
	vsprintf_s(tmp, sizeof(tmp), str, ap);
#else
	vsprintf(tmp, str, ap);
#endif
	log_print(sender, us, tmp);
}

void TheServer::log_printf(LogSender const *sender, microsecond_t us, char const *str, ...)
{
	va_list ap;
	va_start(ap, str);
	log_vprintf(sender, us, str, ap);
	va_end(ap);
}

bool TheServer::is_interrupted() const
{
#ifdef WIN32
	if (_as_service) {
		if (g_srv_status.dwCurrentState == SERVICE_STOP_PENDING) {
			return true;
		}
		if (g_srv_status.dwCurrentState == SERVICE_STOPPED) {
			return true;
		}
	}
	return false;
#else
	if (_sigterm) {
		return true;
	}
	return false;
#endif
}



void TheServer::loop()
{
	while (1) {
		try {

			everysecond();

			if (is_interrupted()) {
				break;
			}

			_loop_event.wait(1000);

		} catch (std::bad_alloc const &e) {
			on_bad_alloc(e, __FILE__, __LINE__);
		}

	}
}
