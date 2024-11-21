
#ifndef __TheServer_h
#define __TheServer_h

// TheServer.h: WindowsサービスとUnixデーモンを隠蔽するクラス

#ifdef WIN32

#include <windows.h>

void WINAPI ServiceMain(DWORD ac, char **av);

#endif

#include <deque>

#include "common/event.h"
#include "common/mutex.h"
#include "common/misc.h"

#include "LogThread.h"
#include "DumpPacket.h"

#include "LogSender.h"

void on_bad_alloc(std::bad_alloc const &e, char const *file, int line);

class TheServer {
	friend class LogThread;
protected:
	bool _as_service;
	void set_as_service(bool f)
	{
		_as_service = f;
	}
	TheServer()
	{
		_as_service = false;
	}
	virtual ~TheServer()
	{
	}

	LogThread _log_thread;
	DumpPacket _dump;

	Event _loop_event;

	virtual void run() = 0;
	virtual bool is_interrupted() const;
	void loop();
	virtual void everysecond() = 0;

public:
	void log_print(LogSender const *sender, microsecond_t us, char const *str);
	void log_vprintf(LogSender const *sender, microsecond_t us, char const *str, va_list ap);
	void log_printf(LogSender const *sender, microsecond_t us, char const *str, ...);
public:
	struct LogData {
		LogOption option;
		Event event;
		Mutex mutex;
		std::list<LogItem> queue;
	} _log_data;
	LogData *get_log_data()
	{
		return &_log_data;
	}
	LogData const *get_log_data() const
	{
		return &_log_data;
	}
	LogOption *get_log_option()
	{
		return &get_log_data()->option;
	}
	LogOption const *get_log_option() const
	{
		return &get_log_data()->option;
	}
public:
#ifdef WIN32
	DWORD ServiceHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData);
#else
	bool _sigterm;
#endif
	virtual int main(bool as_service);
	virtual void set_option(void *ptr)
	{
	}
};

TheServer *get_the_server();




#endif
