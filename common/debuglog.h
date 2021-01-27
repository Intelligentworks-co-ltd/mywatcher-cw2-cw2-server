
#ifndef __debuglog_h
#define __debuglog_h

#ifdef WIN32

void openlog(char const *ident, int option, int facility);
void syslog(int priority, const char *str, ...);
void closelog();

enum Facility {
	LOG_KERNEL            = 0,
	LOG_USER              = 1,
	LOG_MAIL              = 2,
	LOG_DAEMON            = 3,
	LOG_AUTH              = 4,
	LOG_SYSLOG            = 5,
	LOG_LPR               = 6,
	LOG_NEWS              = 7,
	LOG_UUCP              = 8,
	LOG_CRON              = 9,
	LOG_SYSTEM0           = 10,
	LOG_SYSTEM1           = 11,
	LOG_SYSTEM2           = 12,
	LOG_SYSTEM3           = 13,
	LOG_SYSTEM4           = 14,
	LOG_SYSTEM5           = 15,
	LOG_LOCAL0            = 16,
	LOG_LOCAL1            = 17,
	LOG_LOCAL2            = 18,
	LOG_LOCAL3            = 19,
	LOG_LOCAL4            = 20,
	LOG_LOCAL5            = 21,
	LOG_LOCAL6            = 22,
	LOG_LOCAL7            = 23,
};

enum Severity {
	LOG_EMERG             = 0,
	LOG_ALERT             = 1,
	LOG_CRITICAL          = 2,
	LOG_ERROR             = 3,
	LOG_WARNING           = 4,
	LOG_NOTICE            = 5,
	LOG_INFO              = 6,
	LOG_DEBUG             = 7,
};

#else

#include <syslog.h>

#endif

//#include <string>
//
//void send_syslog_debug_message(std::string const &message);
//void send_syslog_debug_message(std::wstring const &message);

#endif
