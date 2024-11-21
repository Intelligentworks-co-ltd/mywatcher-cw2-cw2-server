
#ifndef __cws_event_h
#define __cws_event_h

#include "mutex.h"

#ifdef WIN32

class Event {
private:
	HANDLE _handle;
public:
	Event();
	~Event();
	void wait(int ms = -1);
	void signal();
	//bool signaled() const;
};

#else

class Event {
private:
	Mutex _mutex;
	pthread_cond_t _cond;
public:
	Event();
	~Event();
	void wait(int ms = -1);
	void signal();
	//bool signaled() const;
};

#endif

#endif


