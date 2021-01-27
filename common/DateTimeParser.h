
// Copyright (C) Shinichi 'soramimi' Fuchita.

#ifndef __DateTimeParser_h
#define __DateTimeParser_h

namespace soramimi {
	namespace calendar {
		struct DateTime {
			int year;
			int month;
			int day;
			int hour;
			int minute;
			int second;
			DateTime()
				: year(0)
				, month(0)
				, day(0)
				, hour(0)
				, minute(0)
				, second(0)
			{
			}
			DateTime(int year, int month, int day, int hour, int minute, int second)
				: year(year)
				, month(month)
				, day(day)
				, hour(hour)
				, minute(minute)
				, second(second)
			{
			}
		};

		bool operator == (DateTime const &left, DateTime const &right);
		bool operator < (DateTime const &left, DateTime const &right);
		bool operator > (DateTime const &left, DateTime const &right);
		inline bool operator <= (DateTime const &left, DateTime const &right)
		{
			return !(left > right);
		}
		inline bool operator >= (DateTime const &left, DateTime const &right)
		{
			return !(left < right);
		}

		bool Parse(char const *ptr, DateTime *result);
	}
}

#endif
