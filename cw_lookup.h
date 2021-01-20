
#ifndef __CW_LOOKUP_H
#define __CW_LOOKUP_H

#include "common/thread.h"
#include "cw.h"


#ifdef WIN32
	typedef unsigned int socket_t;
#else
	typedef int socket_t;
#endif


namespace ContentsWatcher {
	class NameLookupThread : public Thread {
	private:
		socket_t sock;
		virtual void run();
	public:
		virtual void stop();
	public:
		NameLookupThread();
	};
}

#endif

