
#include "misc.h"
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include "datetime.h"
#include <time.h>
#include <iosfwd>

#pragma warning(disable:4996)

// ic-version start_eith()
template <typename T> bool startwithicT(T const *foo, T const *bar, T const **next_p)
{
	while (*bar) {
		if (!*foo) {
			return false;
		}

		unsigned int a = *foo;
		unsigned int b = *bar;
		if (a < 0x80) {
			a = toupper(a);
		}
		if (b < 0x80) {
			b = toupper(b);
		}

		if (a != b) {
			return false;
		}
		foo++;
		bar++;
	}
	if (next_p) {
		*next_p = foo;
	}
	return true;
}
bool start_with_ic(char const *foo, char const *bar, char const **next_p)
{
	return startwithicT(foo, bar, next_p);
}
bool start_with_ic(uchar_t const *foo, uchar_t const *bar, uchar_t const **next_p)
{
	return startwithicT(foo, bar, next_p);
}




























template <typename T> bool startwithT(T const *foo, T const *bar, T const **next_p)
{
	while (*bar) {
		if (!*foo) {
			return false;
		}
		if (*foo != *bar) {
			return false;
		}
		foo++;
		bar++;
	}
	if (next_p) {
		*next_p = foo;
	}
	return true;
}


template <typename T> bool endwithT(T const *str, T const *with)
{
	size_t n = ucs::length(str);
	size_t l = ucs::length(with);
	if (n >= l) {
		if (std::char_traits<T>::compare(str + n - l, with, l) == 0) {
			return true;
		}
	}
	return false;
}


bool start_with(char const *foo, char const *bar, char const **next_p)
{
	return startwithT(foo, bar, next_p);
}
bool start_with(uchar_t const *foo, uchar_t const *bar, uchar_t const **next_p)
{
	return startwithT(foo, bar, next_p);
}

bool end_with(char const *str, char const *with)
{
	return endwithT(str, with);
}

bool end_with(uchar_t const *str, uchar_t const *with)
{
	return endwithT(str, with);
}



//std::string ck_to_mac(char const *ck)
//{
//	int m[6];
//	char const *p = strrchr(ck, '_');
//	if (p) {
//		p++;
//		if (sscanf(p, "%02x%02x%02x%02x%02x%02x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
//			char tmp[100];
//			sprintf(tmp, "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3], m[4], m[5]);
//			return tmp;
//		}
//	}
//	return std::string();
//}


namespace misc {

	int wtoi(uchar_t const *p)
	{
		int v = 0;
		if (p) {
			while (iswspace(*p)) {
				p++;
			}
			while (iswdigit(*p)) {
				int n = *p - L'0';
				v = v * 10 + n;
				p++;
			}
		}
		return v;
	}

	long long atoi64(char const *p)
	{
#ifdef WIN32
		return _atoi64(p);
#else
		return strtoll(p, 0, 10);
#endif
	}

}





std::string microsecond_t::tostring()
{
	time_t s = (time_t)(microsecond / 1000000);
	unsigned long u = (unsigned long)(microsecond % 1000000);
	struct tm tm;
	localtime_r(&s, &tm);
	char tmp[100];
	//sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u.%06u"
	//	, tm.tm_year + 1900
	//	, tm.tm_mon + 1
	//	, tm.tm_mday
	//	, tm.tm_hour
	//	, tm.tm_min
	//	, tm.tm_sec
	//	, (int)u
	//	);
	sprintf(tmp, "%04u-%02u-%02u %02u:%02u:%02u"
		, tm.tm_year + 1900
		, tm.tm_mon + 1
		, tm.tm_mday
		, tm.tm_hour
		, tm.tm_min
		, tm.tm_sec
		);
	return tmp;
}


