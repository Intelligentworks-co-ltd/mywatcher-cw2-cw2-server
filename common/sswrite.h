
#ifndef __SSWRITE_H
#define __SSWRITE_H

#include <sstream>
#include "uchar.h"

inline void sswrite(ustringbuffer *out, uchar_t c)
{
	out->put(c);
}

inline void sswrite(ustringbuffer *out, uchar_t const *p)
{
	out->write(p, (std::streamsize)ucs::wcslen(p));
}

inline void sswrite(ustringbuffer *out, uchar_t const *p, size_t l)
{
	out->write(p, (std::streamsize)l);
}

inline void sswrite(ustringbuffer *out, ustring const &s)
{
	sswrite(out, s.c_str(), s.size());
}

//void sswritef(ustringbuffer *out, uchar_t const *f, ...);

inline void sswrite(std::stringstream *out, char c)
{
	out->put(c);
}

inline void sswrite(std::stringstream *out, char const *p)
{
	out->write(p, (std::streamsize)strlen(p));
}

inline void sswrite(std::stringstream *out, char const *p, size_t l)
{
	out->write(p, (std::streamsize)l);
}

inline void sswrite(std::stringstream *out, std::string const &s)
{
	sswrite(out, s.c_str(), s.size());
}

void sswritef(std::stringstream *out, char const *f, ...);


#endif
