
#ifndef __calendar_h
#define __calendar_h

#include <time.h>

// 年月日
struct YMD {
    int year, month, day;
    YMD()
	{
		this->year = 0;
		this->month = 0;
		this->day = 0;
	}
    YMD(int year, int month, int day)
    {
		this->year = year;
		this->month = month;
		this->day = day;
    }
	YMD(time_t t);
	int compare(YMD const &r) const
	{
		if (year < r.year) {
			return -1;
		}
		if (year > r.year) {
			return 1;
		}
		if (month < r.month) {
			return -1;
		}
		if (month > r.month) {
			return 1;
		}
		if (day < r.day) {
			return -1;
		}
		if (day > r.day) {
			return 1;
		}
		return 0;
	}
	bool operator == (YMD const &r) const
	{
		return compare(r) == 0;
	}
	bool operator != (YMD const &r) const
	{
		return compare(r) != 0;
	}
	bool operator <= (YMD const &r) const
	{
		return compare(r) <= 0;
	}
	bool operator >= (YMD const &r) const
	{
		return compare(r) >= 0;
	}
	bool operator < (YMD const &r) const
	{
		return compare(r) < 0;
	}
	bool operator > (YMD const &r) const
	{
		return compare(r) > 0;
	}
};

// 年月日からユリウス通日に変換
int ToJDN(int year, int month, int day);

// ユリウス通日から年月日に変換
YMD ToYMD(int jdn);

#endif
