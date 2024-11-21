
#include "datetime.h"

#include <time.h>
#include "DateTimeParser.h"
#include "calendar.h"


void parse_utc_to_tm(char const *str, struct tm *tm)
{
	soramimi::calendar::DateTime dt;
	soramimi::calendar::Parse(str, &dt);
	long long s = ToJDN(dt.year, dt.month, dt.day);
	s = ((s * 24 + dt.hour) * 60 + dt.minute) * 60 + dt.second;

	long long e = 2440588;//ToJDN(1970, 1, 1);
	e *= 24 * 60 * 60;

	time_t t = (time_t)(s - e);
	localtime_r(&t, tm);
}






