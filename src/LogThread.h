
#ifndef __LogThread_h
#define __LogThread_h

#include "common/thread.h"
#include "common/misc.h"
#include <string>

struct LogOption {
	bool output;
	bool output_syslog;
	bool output_console;
	LogOption()
	{
		output = true;
		output_syslog = false;
		output_console = true;
	}
};

struct LogItem {
	microsecond_t time;
	int threadnumber;
	std::string from;
	std::string text;
	std::string line;
};

void make_log_line(LogItem *item);
void output_log_to_console(LogItem const *item);

class TheServer;

class LogThread : public Thread {
private:
	TheServer *_server;
	int _log_rotate_count;
	std::string _log_path;
	FILE *_fp;
	virtual void run();
public:
	LogThread();
	~LogThread();
	void setup(TheServer *server, std::string const &path, int rotatecount);
	virtual void stop();
};



#endif
