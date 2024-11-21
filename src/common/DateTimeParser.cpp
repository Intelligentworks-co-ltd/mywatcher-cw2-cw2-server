
// Copyright (C) Shinichi 'soramimi' Fuchita.

#include <string.h>
#include <ctype.h>
#include <sstream>
#include <string>

//#include "stricmp.h"

#include "DateTimeParser.h"
#include "holiday.h"
#include "calendar.h"
#include "eq.h"


#ifdef WIN32
#pragma warning(disable:4996)
#endif




namespace soramimi {
	namespace calendar {
		enum State {
			Unknown0,
			DayOfWeek,
			W3CDTF_Month,
			W3CDTF_Day,
			W3CDTF_Hour,
			W3CDTF_Minute,
			W3CDTF_Second,
			W3CDTF_Fragment,
			TimeZoneName,
			TimeZoneH,
			TimeZoneM,
			CTIME_Month,
			CTIME_Day,
			CTIME_Hour,
			CTIME_Minute,
			CTIME_Second,
			CTIME_Year,
			RFC822_Day,
			RFC822_Month,
			RFC822_Year,
			RFC822_Hour,
			RFC822_Minute,
			RFC822_Second,
			ASCTIME_Month,
			ASCTIME_Day,
			ASCTIME_Hour,
			ASCTIME_Minute,
			ASCTIME_Second,
			ASCTIME_Year,
			End,
		};
		static int ParseNumber(char const *str, int *value_p)
		{
			int n;
			int value = 0;
			value = 0;
			for (n = 0; str[n] && isdigit(str[n]); n++) {
				value = value * 10 + (int)(str[n] - '0');
			}
			if (value_p) {
				*value_p = value;
			}
			return n;
		}
		bool Parse(char const *ptr, DateTime *result)
		{
			char const *end = ptr + strlen(ptr);
			enum State state = Unknown0;
			int year = 0;
			int month = 0;
			int day = 0;
			int hour = 0;
			int minute = 0;
			int second = 0;
			int tz_h = 0;
			int tz_m = 0;
			bool tzsign = false;
			while (ptr < end) {
				int val = 0;
				int n = 0;
				while (ptr < end && isspace(*ptr)) {
					ptr++;
				}
				if (isalpha(*ptr)) {
					std::stringstream ss;
					for (n = 0; ptr + n < end && isalpha(ptr[n]); n++) {
						char c = ptr[n];
						ss.write(&c, 1);
					}
					std::string str = ss.str();
					if (eq(str.c_str(), "GMT") == 0) {
						state = TimeZoneName;
					} else {
						if (eqi(str, "SUN")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "MON")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "TUE")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "WED")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "THU")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "FRI")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "SAT")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "SUNDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "MONDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "TUESDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "WEDNESDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "THURSDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "FRIDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "SATURDAY")) { state = DayOfWeek; goto jmp1; }
						if (eqi(str, "JAN")) { val = 1; goto jmp1; }
						if (eqi(str, "FEB")) { val = 2; goto jmp1; }
						if (eqi(str, "MAR")) { val = 3; goto jmp1; }
						if (eqi(str, "APR")) { val = 4; goto jmp1; }
						if (eqi(str, "MAY")) { val = 5; goto jmp1; }
						if (eqi(str, "JUN")) { val = 6; goto jmp1; }
						if (eqi(str, "JUL")) { val = 7; goto jmp1; }
						if (eqi(str, "AUG")) { val = 8; goto jmp1; }
						if (eqi(str, "SEP")) { val = 9; goto jmp1; }
						if (eqi(str, "OCT")) { val = 10; goto jmp1; }
						if (eqi(str, "NOV")) { val = 11; goto jmp1; }
						if (eqi(str, "DEC")) { val = 12; goto jmp1; }
					}
	jmp1:;
					if (val > 0) {
						if (state != RFC822_Month && state != ASCTIME_Month) {
							state = CTIME_Month;
						}
					}
				} else {
					n = ParseNumber(ptr, &val);
				}
				ptr += n;
				char c = (char)0;
				if (ptr < end) {
					c = (unsigned char)*ptr++;
				}
				switch (state) {
				case DayOfWeek:
					if (c == ',') {
						state = RFC822_Day;
					} else if (c == ' ') {
						state = ASCTIME_Month;
					}
					break;
				case Unknown0:
					year = val;
					state = W3CDTF_Month;
					break;
				case W3CDTF_Month:
					month = val;
					state = W3CDTF_Day;
					break;
				case W3CDTF_Day:
					day = val;
					state = W3CDTF_Hour;
					break;
				case W3CDTF_Hour:
					hour = val;
					state = W3CDTF_Minute;
					break;
				case W3CDTF_Minute:
					minute = val;
					state = W3CDTF_Second;
					break;
				case W3CDTF_Second:
					second = val;
					if (c == '.') {
						state = W3CDTF_Fragment;
					} else if (c == '+') {
						tzsign = false;
						state = TimeZoneH;
					} else if (c == '-') {
						tzsign = true;
						state = TimeZoneH;
					}
					break;
				case W3CDTF_Fragment:
					if (c == '+') {
						tzsign = false;
						state = TimeZoneH;
					} else if (c == '-') {
						tzsign = true;
						state = TimeZoneH;
					}
					break;
				case CTIME_Month:
					month = val;
					state = CTIME_Day;
					break;
				case CTIME_Day:
					day = val;
					state = CTIME_Hour;
					break;
				case CTIME_Hour:
					if (val > 48 && year == 0) {
						year = val;
					} else {
						hour = val;
						state = CTIME_Minute;
					}
					break;
				case CTIME_Minute:
					minute = val;
					state = CTIME_Second;
					break;
				case CTIME_Second:
					second = val;
					state = CTIME_Year;
					break;
				case CTIME_Year:
					year = val;
					state = End;
					break;
				case RFC822_Day:
					day = val;
					state = RFC822_Month;
					break;
				case RFC822_Month:
					month = val;
					state = RFC822_Year;
					break;
				case RFC822_Year:
					year = val;
					state = RFC822_Hour;
					break;
				case RFC822_Hour:
					hour = val;
					state = RFC822_Minute;
					break;
				case RFC822_Minute:
					minute = val;
					state = RFC822_Second;
					break;
				case RFC822_Second:
					second = val;
					state = TimeZoneH;
					break;
				case ASCTIME_Month:
					month = val;
					state = ASCTIME_Day;
					break;
				case ASCTIME_Day:
					day = val;
					state = ASCTIME_Hour;
					break;
				case ASCTIME_Hour:
					hour = val;
					state = ASCTIME_Minute;
					break;
				case ASCTIME_Minute:
					minute = val;
					state = ASCTIME_Second;
					break;
				case ASCTIME_Second:
					second = val;
					state = ASCTIME_Year;
					break;
				case ASCTIME_Year:
					year = val;
					state = End;
					break;
				case TimeZoneH:
					if (n == 4) {
						tz_h = val / 100;
						tz_m = val % 100;
						state = End;
					} else {
						tz_h = val;
						if (c == ':') {
							state = TimeZoneM;
						}
					}
					break;
				case TimeZoneM:
					tz_m += val;
					state = End;
					break;
				default:
					break;
				}
				if (c == 0) {
					break;
				}
			}
			if (state != Unknown0) {
				result->year = year;
				result->month = month;
				result->day = day;
				result->hour = hour;
				result->minute = minute;
				result->second = second;
				return true;
			}
			return false;
		}

		bool operator == (DateTime const &left, DateTime const &right)
		{
			if (left.year != right.year) return false;
			if (left.month != right.month) return false;
			if (left.day != right.day) return false;
			if (left.hour != right.hour) return false;
			if (left.minute != right.minute) return false;
			if (left.second != right.second) return false;
			return true;
		}
		bool operator < (DateTime const &left, DateTime const &right)
		{
			long long l = ToJDN(left.year, left.month, left.day);
			long long r = ToJDN(right.year, right.month, right.day);
			l = ((l * 24 + left.hour) * 60 + left.minute) * 60 + left.second;
			r = ((r * 24 + right.hour) * 60 + right.minute) * 60 + right.second;
			return l < r;
		}
		bool operator > (DateTime const &left, DateTime const &right)
		{
			long long l = ToJDN(left.year, left.month, left.day);
			long long r = ToJDN(right.year, right.month, right.day);
			l = ((l * 24 + left.hour) * 60 + left.minute) * 60 + left.second;
			r = ((r * 24 + right.hour) * 60 + right.minute) * 60 + right.second;
			return l > r;
		}
	}
}
