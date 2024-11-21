
#include "misc.h"
#include "sswrite.h"
#include <stdio.h>
#include <stdarg.h>

#pragma warning(disable:4996)

//void sswritef(ustringbuffer *out, uchar_t const *f, ...)
//{
//	uchar_t tmp[10000];
//	va_list ap;
//	va_start(ap, f);
//#ifdef WIN32
//	vswprintf(tmp, f, ap);
//#else
//	vswprintf(tmp, sizeof(tmp), f, ap);
//#endif
//	va_end(ap);
//	sswrite(out, tmp);
//}

void sswritef(std::stringstream *out, char const *f, ...)
{
	char tmp[10000];
	va_list ap;
	va_start(ap, f);
	vsprintf(tmp, f, ap);
	va_end(ap);
	sswrite(out, tmp);
}


