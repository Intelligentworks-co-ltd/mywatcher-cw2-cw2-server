
#ifndef __LogSender_h
#define __LogSender_h

class LogSender {
public:
	virtual ~LogSender()
	{
	}
	std::string ConnectedFrom;
	virtual int get_thread_number() const = 0;
};

#endif
