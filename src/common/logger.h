#ifndef __COMMON_LOGGER_H
#define __COMMON_LOGGER_H

#include <stdarg.h>

#include "misc.h"

class Logger {
public:
	virtual void log_print(microsecond_t us, char const *str) = 0;
	virtual void log_vprintf(microsecond_t us, char const *str, va_list ap) = 0;
	virtual void log_printf(microsecond_t us, char const *str, ...) = 0;
};

#endif
