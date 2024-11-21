
#include "calendar.h"

#ifdef WIN32
#define localtime_r(T, TM) localtime_s(TM, T);
#endif

YMD::YMD(time_t t)
{
	struct tm tm;
	localtime_r(&t, &tm);
	year = tm.tm_year + 1900;
	month = tm.tm_mon + 1;
	day = tm.tm_mday;
}

// 年月日からユリウス通日に変換
int ToJDN(int year, int month, int day)
{
	if (month < 3) {
		month += 9;
		year--;
	} else {
		month -= 3;
	}
	year += 4800;
	int c = (year / 100);
	return (c * 146097 / 4) + ((year - c * 100) * 1461 / 4) + ((153 * month + 2) / 5) + day - 32045;
}

// ユリウス通日から年月日に変換
YMD ToYMD(int jdn)
{
	int y, m, d;
	y = ((jdn * 4 + 128179) / 146097);
	d = ((jdn * 4 - y * 146097 + 128179) / 4) * 4 + 3;
	jdn = (d / 1461);
	d = ((d - jdn * 1461) / 4) * 5 + 2;
	m = (d / 153);
	d = ((d - m * 153) / 5) + 1;
	y = (y - 48) * 100 + jdn;
	if (m < 10) {
		m += 3;
	} else {
		m -= 9;
		y++;
	}
	return YMD(y, m, d);
}

