
#ifndef __ProcessorLoad_h
#define __ProcessorLoad_h

#ifdef WIN32

#include <pdh.h>

class ProcessorLoad_ {
private:
	HQUERY _hQuery;
	HCOUNTER _hCounter;
public:
	bool open();
	bool get(double *result);
	void close();
};

#else

class ProcessorLoad_ {
private:
	long long busy;
	long long idle;
	void update();
public:
	ProcessorLoad_()
	{
		busy = 0;
		idle = 0;
	}
	bool open();
	bool get(double *result);
	void close();
};

#endif


#endif


