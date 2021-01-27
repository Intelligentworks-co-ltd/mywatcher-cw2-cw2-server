

// どれか一つだけ 1 にする
#define USE_MALLOC 0
#define USE_DLMALLOC 0
#define USE_JEMALLOC 1
#define USE_DEBUG_MALLOC 0




unsigned long long diagnostic_allocated_memory = 0;








#ifdef WIN32

#include <windows.h>
class CriticalSection {
public:
	CRITICAL_SECTION _cs;
	CriticalSection()
	{
		InitializeCriticalSection(&_cs);
	}
	~CriticalSection()
	{
		DeleteCriticalSection(&_cs);
	}
	void lock()
	{
		EnterCriticalSection(&_cs);
	}
	void unlock()
	{
		LeaveCriticalSection(&_cs);
	}
};

#else

#include <pthread.h>
class CriticalSection {
public:
	pthread_mutex_t _handle;
	CriticalSection()
	{
		pthread_mutex_init(&_handle, NULL);
	}
	~CriticalSection()
	{
		pthread_mutex_destroy(&_handle);
	}
	void lock()
	{
		pthread_mutex_lock(&_handle);
	}
	void unlock()
	{
		pthread_mutex_unlock(&_handle);
	}
};

#endif




CriticalSection cs_malloc;





#if USE_MALLOC // 標準mallocを使用する




void initialize_mynew()
{
}

void terminate_mynew()
{
}




#endif
#if USE_DLMALLOC // dlmallocを使用する




#include <new>
#include "dlmalloc.h"

struct memory_tag_t {
	unsigned long size;
};

void initialize_mynew()
{
}

void terminate_mynew()
{
}

static inline void *mynew(size_t size)
{
	char *p = (char *)dlmalloc(sizeof(memory_tag_t) + size);
	if (!p) {
		static const std::bad_alloc nomem;
		throw nomem;
	}

	memory_tag_t *h = (memory_tag_t *)p;
	h->size = size;

	cs_malloc.lock();
	diagnostic_allocated_memory += size;
	cs_malloc.unlock();

	return (void *)(p + sizeof(memory_tag_t));
}

static inline void mydelete(void *p)
{
	if (p) {
		p = (char *)p - sizeof(memory_tag_t);
		memory_tag_t *h = (memory_tag_t *)p;
		cs_malloc.lock();
		diagnostic_allocated_memory -= h->size;
		cs_malloc.unlock();
		dlfree(p);
	}
}

void *operator new(size_t size)
{
	return mynew(size);
}

void operator delete(void *p)
{
	mydelete(p);
}

void *operator new[](size_t size)
{
	return mynew(size);
}

void operator delete[](void *p)
{
	mydelete(p);
}




#endif
#if USE_JEMALLOC // jemallocを使用する




#include <new>

struct memory_tag_t {
	unsigned long size;
};

extern "C" {
	bool init_jemalloc(void);
	void term_jemalloc();
	void *jemalloc(size_t size);
	void *jevalloc(size_t size);
	void *jecalloc(size_t num, size_t size);
	void *jerealloc(void *ptr, size_t size);
	void jefree(void *ptr);
}

void initialize_mynew()
{
	init_jemalloc();
}

void terminate_mynew()
{
	term_jemalloc();
}

static inline void *mynew(size_t size)
{
	char *p = (char *)jemalloc(sizeof(memory_tag_t) + size);
	if (!p) {
		static const std::bad_alloc nomem;
		throw nomem;
	}

	memory_tag_t *h = (memory_tag_t *)p;
	h->size = size;

	cs_malloc.lock();
	diagnostic_allocated_memory += size;
	cs_malloc.unlock();

	return (void *)(p + sizeof(memory_tag_t));
}

static inline void mydelete(void *p)
{
	if (p) {
		p = (char *)p - sizeof(memory_tag_t);
		memory_tag_t *h = (memory_tag_t *)p;

		cs_malloc.lock();
		diagnostic_allocated_memory -= h->size;
		cs_malloc.unlock();

		jefree(p);
	}
}

void *operator new(size_t size)
{
	return mynew(size);
}

void operator delete(void *p)
{
	mydelete(p);
}

void *operator new[](size_t size)
{
	return mynew(size);
}

void operator delete[](void *p)
{
	mydelete(p);
}




#endif
#if USE_DEBUG_MALLOC // malloc with debug




#include <new>
#include <stdlib.h>
#include "../common/mutex.h"

struct memory_tag_t {
	unsigned long size;
	memory_tag_t *next;
	memory_tag_t *prev;
};

memory_tag_t memory_root = { 0, 0, 0 };
CriticalSection memory_mutex;

void initialize_mynew()
{
}

void terminate_mynew()
{
}

static inline void *mynew(size_t size)
{
	size_t s = size;
	if (s >= 0 && s < 2000) {
		s *= 10;
	}
	char *p = (char *)malloc(sizeof(memory_tag_t) + s);
	if (!p) {
		static const std::bad_alloc nomem;
		throw nomem;
	}

	cs_malloc.lock();

	if (!memory_root.next) {
		memory_root.next = &memory_root;
		memory_root.prev = &memory_root;
	}

	memory_tag_t *h = (memory_tag_t *)p;
	h->size = size;

	diagnostic_allocated_memory += size;

	h->next = memory_root.next;
	h->prev = &memory_root;
	memory_root.next->prev = h;
	memory_root.next = h;

	cs_malloc.unlock();

	return (void *)(p + sizeof(memory_tag_t));
}

static inline void mydelete(void *p)
{
	if (p) {
		p = (char *)p - sizeof(memory_tag_t);
		memory_tag_t *h = (memory_tag_t *)p;
		diagnostic_allocated_memory -= h->size;

		cs_malloc.lock();

		h->next->prev = h->prev;
		h->prev->next = h->next;

		cs_malloc.unlock();

		free(p);
	}
}

void *operator new(size_t size)
{
	return mynew(size);
}

void operator delete(void *p)
{
	mydelete(p);
}

void *operator new[](size_t size)
{
	return mynew(size);
}

void operator delete[](void *p)
{
	mydelete(p);
}




#endif
