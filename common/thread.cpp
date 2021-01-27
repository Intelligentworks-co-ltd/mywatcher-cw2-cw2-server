#include "thread.h"

#ifdef WIN32
unsigned int __stdcall Thread::run_(void *arg)
{
	Thread *me = (Thread *)arg;
	me->run();
	me->_running = false;
	return 0;
}
#else
void *Thread::run_(void *arg)
{
	Thread *me = (Thread *)arg;
	me->run();
	me->_running = false;
	return 0;
}
#endif

void Thread::start()
{
#ifdef WIN32
	_thread_handle = (HANDLE)_beginthreadex(0, 0, run_, this, CREATE_SUSPENDED, 0);
	ResumeThread(_thread_handle);
#else
	pthread_create(&_thread_handle, NULL, run_, this);
#endif
	_running = true;
}

void Thread::stop()
{
	_terminate = true;
}

void Thread::join()
{
	if (_running) {
#ifdef WIN32
		WaitForSingleObject(_thread_handle, INFINITE);
#else
		pthread_join(_thread_handle, 0);
#endif
	}
}

void Thread::detach()
{
	if (_running) {
#ifdef WIN32
		CloseHandle(_thread_handle);
		_thread_handle = 0;
#else
		pthread_detach(_thread_handle);
		_thread_handle = 0;
#endif
	}
	_running = false;
}
