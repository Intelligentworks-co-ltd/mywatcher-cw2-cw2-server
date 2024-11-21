
#ifndef __DumpThread_h
#define __DumpThread_h

#include "common/misc.h"
#include <string>
#include <list>

namespace ContentsWatcher {
	struct CapturedPacket;
}

class TheServer;

class DumpPacket {
private:
	TheServer *_server;
	int _log_rotate_count;
	std::string _log_path;
	int _fd;
	void rotate();
public:
	DumpPacket();
	~DumpPacket();
	void setup(TheServer *server, std::string const &path, int rotatecount);
	void add(ContentsWatcher::CapturedPacket const *packet);
};



#endif
