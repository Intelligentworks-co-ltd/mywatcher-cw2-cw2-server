
#ifndef __CWSSERVER_MISC_H
#define __CWSSERVER_MISC_H

#include "uchar.h"
#include <vector>

#include "mem.h"




bool start_with(char const *foo, char const *bar, char const **next_p = 0);
bool start_with(uchar_t const *foo, uchar_t const *bar, uchar_t const **next_p = 0);
bool end_with(char const *str, char const *with);
bool end_with(uchar_t const *str, uchar_t const *with);

bool start_with_ic(char const *foo, char const *bar, char const **next_p = 0);
bool start_with_ic(uchar_t const *foo, uchar_t const *bar, uchar_t const **next_p = 0);


static inline void consume(std::vector<unsigned char> *vec, size_t n)
{
	if (n < vec->size()) {
		unsigned char *p = &vec->at(0);
		size_t l = vec->size() - n;
		memmove(p, p + n, l);
		vec->resize(l);
	} else {
		vec->clear();
	}
}




#include <vector>
#include <sstream>

#if _MSC_VER < 1400
#define _countof(ARRAY) (sizeof(ARRAY) / sizeof(*ARRAY))
#endif

inline void push2(std::vector<unsigned char> *vec, unsigned short val)
{
	vec->push_back((unsigned char)(val >> 8));
	vec->push_back((unsigned char)val);
}

inline void push4(std::vector<unsigned char> *vec, unsigned long val)
{
	vec->push_back((unsigned char)(val >> 24));
	vec->push_back((unsigned char)(val >> 16));
	vec->push_back((unsigned char)(val >> 8));
	vec->push_back((unsigned char)val);
}


static inline int xdigit(int c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	return -1;
}



std::string ck_to_mac(char const *ck);


namespace misc {
	long long atoi64(char const *p);
}



#include "sswrite.h"



class microsecond_t {
private:
	unsigned long long microsecond;
public:
	microsecond_t()
	{
		microsecond = 0;
	}
	microsecond_t(unsigned long long us)
	{
		microsecond = us;
	}
	operator unsigned long long () const
	{
		return microsecond;
	}
	std::string tostring();
};


#endif
