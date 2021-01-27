
#ifndef __EQ_H
#define __EQ_H

#include "uchar.h"

#pragma warning(disable:4996)

static inline bool eq(char const *a, char const *b)
{
	return strcmp(a, b) == 0;
}

static inline bool eq(uchar_t const *a, uchar_t const *b)
{
	return ucs::wcscmp(a, b) == 0;
}

static inline bool eq(std::string const &a, char const *b)
{
	return strcmp(a.c_str(), b) == 0;
}

static inline bool eq(std::string const &a, std::string const &b)
{
	return a == b;
}

static inline bool eq(ustring const &a, uchar_t const *b)
{
	return ucs::wcscmp(a.c_str(), b) == 0;
}

static inline bool eq(ustring const &a, ustring const &b)
{
	return a == b;
}

static inline bool eqi(char const *a, char const *b)
{
	return ucs::stricmp(a, b) == 0;
}

static inline bool eqi(uchar_t const *a, uchar_t const *b)
{
	return ucs::wcsicmp(a, b) == 0;
}

static inline bool eqi(std::string const &a, char const *b)
{
	return ucs::stricmp(a.c_str(), b) == 0;
}

static inline bool eqi(std::string const &a, std::string const &b)
{
	return ucs::stricmp(a.c_str(), b.c_str()) == 0;
}

static inline bool eqi(ustring const &a, uchar_t const *b)
{
	return ucs::wcsicmp(a.c_str(), b) == 0;
}

static inline bool eqi(ustring const &a, ustring const &b)
{
	return ucs::wcsicmp(a.c_str(), b.c_str()) == 0;
}

#endif

