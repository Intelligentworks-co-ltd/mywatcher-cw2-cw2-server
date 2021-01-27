
#ifndef __COMBINEPATH_H
#define __COMBINEPATH_H

#include "uchar.h"

#ifdef WIN32
#define DEFAULT_PATH_SEPARATOR '\\'
#else
#define DEFAULT_PATH_SEPARATOR '/'
#endif

ustring CombinePath(uchar_t const *left, uchar_t const *right, uchar_t separator = DEFAULT_PATH_SEPARATOR);
std::string CombinePath(char const *left, char const *right, char separator = DEFAULT_PATH_SEPARATOR);

#endif

