
#ifndef __datetime_h
#define __datetime_h

void parse_utc_to_tm(char const *str, struct tm *tm);

#ifdef WIN32
#define localtime_r(T, TM) localtime_s(TM, T);
#endif

#endif



