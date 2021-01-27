
#ifndef __thread_h
#define __thread_h

#include <stdarg.h>

#ifdef WIN32
	#include <windows.h>
	#include <process.h>
#else
	#include <pthread.h>
#endif



class Thread {
protected:
	volatile bool _terminate;
	bool _running;
#ifdef WIN32
	HANDLE _thread_handle;
	static unsigned int __stdcall run_(void *arg);
#else
	pthread_t _thread_handle;
	static void *run_(void *arg);
#endif

public:
	Thread()
	{
		_terminate = false;
		_running = false;
	}
	virtual ~Thread()
	{
		detach();
	}
	void log_print(char const *str);
	void log_vprintf(char const *str, va_list ap);
	void log_printf(char const *str, ...);
protected:
	virtual void run() = 0;
	virtual bool interrupted() const
	{
		return _terminate;
	}
public:
	virtual void start();
	virtual void stop();
	virtual void join();
	virtual void detach();
	bool running() const
	{
		return _running;
	}
};



#endif
